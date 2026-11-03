import unittest
from email.message import Message
from license_server import request_ip

class GatewayTests(unittest.TestCase):
    def headers(self,token='fixture-secret',ip='198.51.100.4'):
        headers=Message();headers['X-Capri-Gateway']=token;headers['X-Capri-Client-IP']=ip
        headers['CF-Connecting-IP']='198.51.100.200'
        return headers
    def test_authenticated_ip_wins_over_proxy_ip(self):
        self.assertEqual(request_ip(self.headers(),'127.0.0.1','fixture-secret'),'198.51.100.4')
    def test_forged_headers_fail_closed(self):
        for token,peer,secret in [('forged','127.0.0.1','fixture-secret'),('fixture-secret','203.0.113.1','fixture-secret'),('fixture-secret','127.0.0.1',None)]:
            with self.assertRaises(ValueError):request_ip(self.headers(token),peer,secret)
    def test_duplicate_and_invalid_ips_are_rejected(self):
        headers=self.headers();headers['X-Capri-Client-IP']='198.51.100.5'
        with self.assertRaises(ValueError):request_ip(headers,'127.0.0.1','fixture-secret')
        with self.assertRaises(ValueError):request_ip(self.headers(ip='not-an-ip'),'127.0.0.1','fixture-secret')

if __name__=='__main__':unittest.main()
