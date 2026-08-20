#
# IP change from IPv4 to IPv6.
#
# See inc_ipchange.py for the scenario, the phases and what the test
# asserts. 221_ip_change_ipv6_to_ipv4.py is the same test in the
# opposite direction.
#
import inc_ipchange as ipc

test_param = ipc.build_test(ipc.IPV4, ipc.IPV6)
