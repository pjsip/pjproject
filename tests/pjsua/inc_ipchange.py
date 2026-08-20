#
# Shared driver for the IPv4 <-> IPv6 IP change tests in scripts-run/.
#
# Scenario: a dual-stack pjsua ("cli") loses the network it was
# registered and calling over and comes up on the other address family,
# which is what happens to a softphone that moves between an IPv4-only
# and an IPv6-only network.
#
# This follows the procedure in
# https://docs.pjsip.org/en/latest/specific-guides/network_nat/ip_change.html
#
#   - a same-family change needs nothing but pjsua_handle_ip_change();
#   - a change that crosses address families additionally needs a
#     transport of the new family to exist, the account's
#     ipv6_sip_use/ipv6_media_use moved to that family (with
#     disable_reg_on_modify set, so pjsua_acc_modify() does not send a
#     REGISTER over the transport that just died), and only then
#     pjsua_handle_ip_change().
#
# The pjsua application exposes both as its "ip_change" command: bare
# for the same-family case, and with a 4 or 6 argument for the
# cross-family case (see app_handle_ip_change() in pjsua_app.c). The
# transport of the new family comes from starting cli with --ipv6, which
# creates an IPv4 and an IPv6 UDP listener up front.
#
# Phases, per test:
#
#   1. add the account, pin it to the starting family ("ip_change <af>"),
#      place a call, then a bare "ip_change" -- registration, call and
#      media must survive on that family.
#   2. the cross-family switch: the call is torn down first (see the
#      limitation note below), then "ip_change <other af>" moves the
#      account over, and a fresh call must come up with signalling and
#      media of the new family.
#   3. a bare "ip_change" on the new family too, so the same-family path
#      is covered on both, then hang up.
#
# Limitation, deliberately not asserted: a call cannot presently be
# carried across the family switch. With the account moved to
# USE_IPV6_ONLY (or back), pjsua's IP-change re-INVITE for the existing
# dialog fails before it is ever sent --
#   pjsua_call.c  Unable to send re-INVITE: Unsuitable transport selected
#                 (PJSIP_ETPNOTSUITABLE)
#   pjsua_app.c   IP change progress fail: Unsuitable transport selected
# -- even when the dialog's remote target is a name that resolves to both
# families, as it is here. The transaction rejects the destination it
# already holds and finds no candidate of the required family. Until that
# is addressed in pjsip, phase 2 hangs up first; the family really having
# moved is then proven by the next call.
#
# 220_ip_change_ipv4_to_ipv6.py and 221_ip_change_ipv6_to_ipv4.py are
# mirror images of each other.
#
# Test rig: the account keeps one registrar URI, "sip:localhost:PORT",
# across both families -- that is the point, since the switch has to be
# driven by the account's IP version preference and not by swapping in a
# different-family URI. So "srv" runs with --ipv6-port-offset=0, putting
# its IPv4 and IPv6 listeners on the same port, and forces its Contact
# to "sip:localhost:PORT" as well (--id plus --contact): a dialog's
# remote target is the peer's Contact, so an IP literal there would pin
# the call to the family it started on and the re-INVITE after the switch
# could never be delivered. A peer published under a name reachable over
# both families is what makes the call survive -- the same thing that
# makes it work against a real dual-stack proxy or SBC.
#
# The local address family is asserted on the Contact header of the
# logged REGISTER rather than on a specific address: pjsua fills the host
# part from pj_gethostip(), so the value depends on what addresses the
# host happens to have, but an IPv6 host is always rendered in brackets
# (the IPv6reference production of the SIP URI grammar) and an IPv4 host
# never is. The family a request actually goes out on is read off pjsua's
# "TX ... to UDP <addr>:<port>" log line, and the media family off the
# SDP connection line -- meaningful here because the documented procedure
# moves ipv6_media_use along with ipv6_sip_use. That media carries
# packets at all is checked with RFC 2833 DTMF, which travels in the RTP
# stream itself.
#
import random
import re
import socket

import inc_const as const
import inc_util as util
from inc_cfg import *


IPV4 = 4
IPV6 = 6

# pjsua puts its IPv6 UDP listener on (--local-port + 10), see the
# "Add UDP IPv6 transport" block in pjsua_app.c.
IPV6_PORT_OFFSET = 10

PJSUA_ARGS = "--null-audio --max-calls=4 --no-tcp"

# One ip_change covers a listener restart plus a re-REGISTER and a
# re-INVITE round trip, and restart_listener() reschedules itself
# (after pjsua_ip_change_param.restart_lis_delay) for as long as the
# listener cannot be re-bound. On loopback all of it lands within
# milliseconds, so this is only a ceiling for a badly loaded runner.
IP_CHANGE_TIMEOUT = 60

# pjsua renders an IPv6 host in brackets and an IPv4 host as dotted
# quad, which is enough to pin down the family of pjsua's own Contact
# without having to predict the address pj_gethostip() will pick.
CONTACT_PAT = {
    IPV4: r"Contact:.*@\d+\.\d+\.\d+\.\d+",
    IPV6: r"Contact:.*@\[[0-9a-f:.]+\]",
}

SDP_CONN_PAT = {IPV4: r"^c=IN IP4 ", IPV6: r"^c=IN IP6 "}

# How pjsua renders the peer address in its "TX ... to UDP <addr>:<port>"
# message log line, which is where the family a request actually went out
# on can be read off.
DEST_HOST_PAT = {IPV4: r"\d+\.\d+\.\d+\.\d+", IPV6: r"\[[0-9a-f:.]+\]"}

FAMILY_NAME = {IPV4: "IPv4", IPV6: "IPv6"}

# The peer is published under this name so that one URI -- registrar,
# call target and the peer's own Contact -- works over either family.
REGISTRAR_HOST = "localhost"


def tx_dest_pat(method, af):
    """Match the log line of an outgoing 'method' request sent to a peer
    of address family 'af'."""
    return r"TX .*%s.* to UDP %s:" % (method, DEST_HOST_PAT[af])


def host_has_ipv6():
    """Return True if this host can actually use IPv6 loopback. A build
    with PJ_HAS_IPV6=1 is not enough: some CI containers are started
    without IPv6 at all, and there pjsua's UDP6 listener cannot come up.
    """
    if not socket.has_ipv6:
        return False
    try:
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    except socket.error:
        return False
    try:
        s.bind(("::1", 0))
    except socket.error:
        return False
    finally:
        s.close()
    return True


def registrar_host_is_dual_stack():
    """Return True if REGISTRAR_HOST resolves to both families, which is
    what lets one registrar URI stand for both of them. Modern glibc and
    macOS answer "localhost" with 127.0.0.1 and ::1 whatever /etc/hosts
    says, but that is not something to bet a test on silently.
    """
    families = set()
    try:
        for ai in socket.getaddrinfo(REGISTRAR_HOST, None, 0,
                                     socket.SOCK_DGRAM):
            families.add(ai[0])
    except socket.error:
        return False
    return socket.AF_INET in families and socket.AF_INET6 in families


def _port_free(af, port):
    try:
        s = socket.socket(af, socket.SOCK_DGRAM)
    except socket.error:
        return False
    try:
        if af == socket.AF_INET6:
            # pjlib sets IPV6_V6ONLY on its IPv6 sockets (see
            # PJ_SOCK_HAS_IPV6_V6ONLY in sock_bsd.c), so probe the same
            # way -- a dual-stack probe socket would report a conflict
            # against an unrelated IPv4-only listener.
            s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
            s.bind(("::", port))
        else:
            s.bind(("0.0.0.0", port))
    except socket.error:
        return False
    finally:
        s.close()
    return True


def alloc_ports():
    """Reserve the base ports for the two instances, or None.

    Returns (srv_port, cli_port). srv binds srv_port on both families
    (--ipv6-port-offset=0), and cli binds cli_port on IPv4 and
    (cli_port + IPV6_PORT_OFFSET) on IPv6. Candidates are multiples of
    100 so that cli's IPv6 listener cannot land on srv's port.

    InstanceParam's own port picker is bypassed because it only checks
    the port itself, on IPv4, which would leave the rest to chance.
    """
    picked = []
    for _ in range(50):
        port = random.randrange(DEFAULT_START_SIP_PORT, 60000, 100)
        if port in picked:
            continue
        if not (_port_free(socket.AF_INET, port) and
                _port_free(socket.AF_INET6, port) and
                _port_free(socket.AF_INET6, port + IPV6_PORT_OFFSET)):
            continue
        picked.append(port)
        if len(picked) == 2:
            return picked[0], picked[1]
    return None


def srv_uri(srv_port):
    """The one registrar/call URI, valid over either family."""
    return "sip:%s:%d" % (REGISTRAR_HOST, srv_port)


def add_account(cli, srv_port, title):
    """Add the account and wait until it is registered. The address
    family is whatever the resolver picks at this point -- phase 1 pins
    it right after."""
    cli.send('+a "sip:cli@ipchange.test" "%s" "*" cli secret' %
             srv_uri(srv_port))
    cli.expect("registration success", title=title + " registered")


def check_media_flow(sender, receiver, digits, title):
    """Verify RTP is really flowing, by sending RFC 2833 DTMF: the digits
    travel in the RTP stream itself, so the receiver can only log them if
    the packets arrive. Same idiom as check_media() in mod_call.py."""
    sender.send("# " + digits)
    for d in digits:
        receiver.expect(const.RX_DTMF + d, title=title + " DTMF " + d)


def make_call(cli, srv, srv_port, af, title):
    cli.send("call new " + srv_uri(srv_port))
    # Everything up to the CALLING state change is logged after the
    # INVITE itself, and expect() drops whatever precedes a match, so the
    # request has to be asserted on before the state.
    cli.expect(tx_dest_pat("INVITE", af), title=title + " INVITE peer")
    cli.expect(SDP_CONN_PAT[af], title=title + " offered media family")
    cli.expect(const.STATE_CALLING, title=title + " calling")
    # Media comes up while the call is still CONNECTING (pjsua applies
    # the negotiated SDP on the 200, and only reaches CONFIRMED once it
    # has sent the ACK).
    cli.expect(const.MEDIA_ACTIVE, title=title + " media")
    cli.expect(const.STATE_CONFIRMED, title=title + " confirmed")
    srv.expect(const.MEDIA_ACTIVE, title=title + " media (srv)")
    srv.expect(const.STATE_CONFIRMED, title=title + " confirmed (srv)")
    check_media_flow(cli, srv, "12", title + " cli->srv")
    check_media_flow(srv, cli, "34", title + " srv->cli")


def hangup_all(cli, srv, title):
    cli.send("call hangup_all")
    cli.expect(const.STATE_DISCONNECTED, title=title + " disconnected")
    srv.expect(const.STATE_DISCONNECTED, title=title + " disconnected (srv)")


def ip_change(cli, srv, af, have_call, title, switch_family=False):
    """Run the ip_change command and verify the account (and the call, if
    'have_call') comes back on address family 'af'.

    With switch_family, ip_change is given the target IP version, which
    makes pjsua move every account over to it (ipv6_sip_use and
    ipv6_media_use, with disable_reg_on_modify, applied via
    pjsua_acc_modify) before handling the change -- the documented
    procedure for a network change that crosses address families.
    Without it, the plain same-family ip_change is run.
    """
    cli.send(("ip_change %d" % af) if switch_family else "ip_change")

    if switch_family:
        cli.expect("Accounts switched to IPv%d only" % af,
                   title=title + " account switch",
                   timeout=IP_CHANGE_TIMEOUT)

    # The listener restart is what re-creates the UDP/UDP6 sockets; it
    # is reported once per transport.
    cli.expect("IP change progress report : restart transport",
               title=title + " restart listener", timeout=IP_CHANGE_TIMEOUT)

    # Then the account re-REGISTERs. Both halves of "over the right
    # family" are checked: the peer the REGISTER is sent to, and the
    # Contact it advertises.
    cli.expect(tx_dest_pat("REGISTER", af),
               title=title + " re-REGISTER peer", timeout=IP_CHANGE_TIMEOUT)
    cli.expect(CONTACT_PAT[af], title=title + " re-REGISTER contact",
               timeout=IP_CHANGE_TIMEOUT)
    cli.expect("registration success", title=title + " re-registered",
               timeout=IP_CHANGE_TIMEOUT)

    if have_call:
        # The re-INVITE is logged between the two lines below, so this is
        # where its offered media family can be read off.
        cli.expect("send re-INVITE with flags .* triggered by IP change",
                   title=title + " re-INVITE triggered",
                   timeout=IP_CHANGE_TIMEOUT)
        cli.expect(tx_dest_pat("INVITE", af),
                   title=title + " re-INVITE peer", timeout=IP_CHANGE_TIMEOUT)
        cli.expect(SDP_CONN_PAT[af], title=title + " re-offered media family",
                   timeout=IP_CHANGE_TIMEOUT)
        cli.expect("IP change progress report : reinvite call for account",
                   title=title + " re-INVITE", timeout=IP_CHANGE_TIMEOUT)

    cli.expect("IP address change handling completed",
               title=title + " completed", timeout=IP_CHANGE_TIMEOUT)

    if have_call:
        # pjsua considers the IP change handled once the re-INVITE has
        # been *sent*, so the media only comes back afterwards.
        cli.expect(const.MEDIA_ACTIVE, title=title + " media after re-INVITE",
                   timeout=IP_CHANGE_TIMEOUT)
        srv.expect(const.MEDIA_ACTIVE,
                   title=title + " media after re-INVITE (srv)",
                   timeout=IP_CHANGE_TIMEOUT)
        # ...and being active is not the same as carrying packets.
        check_media_flow(cli, srv, "56", title + " cli->srv")
        check_media_flow(srv, cli, "78", title + " srv->cli")


def build_test(first_af, second_af):
    """Build the TestParam for an IP change from 'first_af' to
    'second_af'."""
    title = "IP change %s -> %s" % (FAMILY_NAME[first_af],
                                    FAMILY_NAME[second_af])
    ports = alloc_ports()

    # Fall back to letting InstanceParam pick when nothing could be
    # reserved; the test is skipped below in that case anyway.
    srv_port = ports[0] if ports else 0
    cli_port = ports[1] if ports else 0

    # One server answering on both families at the same port, published
    # under a name that resolves to both, so a call can be re-INVITEd
    # across a family switch. --id is what makes pjsua create the account
    # that --contact applies to. Distinct RTP base ports keep the two
    # instances from walking up from "Address already in use".
    srv = InstanceParam("srv",
                        PJSUA_ARGS + " --ipv6 --ipv6-port-offset=0"
                        " --auto-answer=200 --rtp-port=4000"
                        " --id=sip:srv@ipchange.test"
                        " --contact=" + srv_uri(srv_port),
                        sip_port=srv_port)
    # cli deliberately starts without an account: adding it at runtime
    # puts the REGISTER after the framework has attached to the telnet
    # CLI, so the Contact it carries is observable via expect(). --ipv6
    # is what gives it a transport of each family to switch between.
    cli = InstanceParam("cli", PJSUA_ARGS + " --ipv6 --rtp-port=4100",
                        sip_port=cli_port)

    def test_func(t):
        srv_proc, cli_proc = t.process[0], t.process[1]
        port = t.inst_params[0].sip_port

        # Phase 1: come up on the starting family, with a call on it, and
        # survive a same-family IP change.
        add_account(cli_proc, port, "phase1")
        ip_change(cli_proc, srv_proc, first_af, False, "phase1 pin",
                  switch_family=True)
        make_call(cli_proc, srv_proc, port, first_af, "phase1")
        ip_change(cli_proc, srv_proc, first_af, True, "phase1 ip_change")

        # Phase 2: the network moves to the other family.
        hangup_all(cli_proc, srv_proc, "phase2")
        ip_change(cli_proc, srv_proc, second_af, False, "phase2 switch",
                  switch_family=True)
        make_call(cli_proc, srv_proc, port, second_af, "phase2")

        # Phase 3: survive a same-family IP change on the new family too.
        ip_change(cli_proc, srv_proc, second_af, True, "phase3 ip_change")
        hangup_all(cli_proc, srv_proc, "phase3")

    test_param = TestParam(title, [srv, cli], func=test_func)

    # Needs IPv6 both on the host and in the build: pjsua exits at
    # startup if --ipv6 is given to a build without IPv6 support, which
    # the framework can only report as a premature EOF.
    if (ports is None or not host_has_ipv6() or
            not registrar_host_is_dual_stack() or not util.has_ipv6(G_EXE)):
        test_param.skip = True

    return test_param
