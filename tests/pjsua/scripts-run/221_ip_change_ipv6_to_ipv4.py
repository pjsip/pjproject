#
# IP change from IPv6 to IPv4.
#
# See inc_ipchange.py for the scenario, the phases and what the test
# asserts. 220_ip_change_ipv4_to_ipv6.py is the same test in the
# opposite direction.
#
import inc_ipchange as ipc

test_param = ipc.build_test(ipc.IPV6, ipc.IPV4)
