/* $Id$ */
/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pjsua2/types.hpp>
#include <pjsua2/siptypes.hpp>
#include "util.hpp"

using namespace pj;
using namespace std;

#define THIS_FILE	"siptypes.cpp"

///////////////////////////////////////////////////////////////////////////////
namespace pj
{
void readIntVector( ContainerNode &node,
                    const string &array_name,
                    IntVector &v) throw(Error)
{
    ContainerNode array_node = node.readArray(array_name);
    v.resize(0);
    while (array_node.hasUnread()) {
	v.push_back((int)array_node.readNumber());
    }
}

void writeIntVector(ContainerNode &node,
                    const string &array_name,
                    const IntVector &v) throw(Error)
{
    ContainerNode array_node = node.writeNewArray(array_name);
    for (unsigned i=0; i<v.size(); ++i) {
	array_node.writeNumber("", v[i]);
    }
}

void readQosParams( ContainerNode &node,
                    pj_qos_params &qos) throw(Error)
{
    ContainerNode this_node = node.readContainer("qosParams");

    NODE_READ_NUM_T( this_node, pj_uint8_t, qos.flags);
    NODE_READ_NUM_T( this_node, pj_uint8_t, qos.dscp_val);
    NODE_READ_NUM_T( this_node, pj_uint8_t, qos.so_prio);
    NODE_READ_NUM_T( this_node, pj_qos_wmm_prio, qos.wmm_prio);
}

void writeQosParams( ContainerNode &node,
                     const pj_qos_params &qos) throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("qosParams");

    NODE_WRITE_NUM_T( this_node, pj_uint8_t, qos.flags);
    NODE_WRITE_NUM_T( this_node, pj_uint8_t, qos.dscp_val);
    NODE_WRITE_NUM_T( this_node, pj_uint8_t, qos.so_prio);
    NODE_WRITE_NUM_T( this_node, pj_qos_wmm_prio, qos.wmm_prio);
}

void readSipHeaders( const ContainerNode &node,
                     const string &array_name,
                     SipHeaderVector &headers) throw(Error)
{
    ContainerNode headers_node = node.readArray(array_name);
    headers.resize(0);
    while (headers_node.hasUnread()) {
	SipHeader hdr;

	ContainerNode header_node = headers_node.readContainer("header");
	hdr.hName = header_node.readString("hname");
	hdr.hValue = header_node.readString("hvalue");
	headers.push_back(hdr);
    }
}

void writeSipHeaders(ContainerNode &node,
                     const string &array_name,
                     const SipHeaderVector &headers) throw(Error)
{
    ContainerNode headers_node = node.writeNewArray(array_name);
    for (unsigned i=0; i<headers.size(); ++i) {
	ContainerNode header_node = headers_node.writeNewContainer("header");
	header_node.writeString("hname", headers[i].hName);
	header_node.writeString("hvalue", headers[i].hValue);
    }
}

} // namespace
///////////////////////////////////////////////////////////////////////////////

AuthCredInfo::AuthCredInfo()
: scheme("digest"), realm("*"), dataType(0)
{
}

AuthCredInfo::AuthCredInfo(const string &param_scheme,
			   const string &param_realm,
			   const string &param_user_name,
			   const int param_data_type,
			   const string param_data)
: scheme(param_scheme), realm(param_realm), username(param_user_name),
  dataType(param_data_type), data(param_data)
{
}

void AuthCredInfo::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AuthCredInfo");

    NODE_READ_STRING( this_node, scheme);
    NODE_READ_STRING( this_node, realm);
    NODE_READ_STRING( this_node, username);
    NODE_READ_INT   ( this_node, dataType);
    NODE_READ_STRING( this_node, data);
    NODE_READ_STRING( this_node, akaK);
    NODE_READ_STRING( this_node, akaOp);
    NODE_READ_STRING( this_node, akaAmf);
}

void AuthCredInfo::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AuthCredInfo");

    NODE_WRITE_STRING( this_node, scheme);
    NODE_WRITE_STRING( this_node, realm);
    NODE_WRITE_STRING( this_node, username);
    NODE_WRITE_INT   ( this_node, dataType);
    NODE_WRITE_STRING( this_node, data);
    NODE_WRITE_STRING( this_node, akaK);
    NODE_WRITE_STRING( this_node, akaOp);
    NODE_WRITE_STRING( this_node, akaAmf);
}

///////////////////////////////////////////////////////////////////////////////

TlsConfig::TlsConfig()
{
    pjsip_tls_setting ts;
    pjsip_tls_setting_default(&ts);
    this->fromPj(ts);
}

pjsip_tls_setting TlsConfig::toPj() const
{
    pjsip_tls_setting ts;

    ts.ca_list_file	= str2Pj(this->CaListFile);
    ts.cert_file	= str2Pj(this->certFile);
    ts.privkey_file	= str2Pj(this->privKeyFile);
    ts.password		= str2Pj(this->password);
    ts.method		= this->method;
    ts.ciphers_num	= this->ciphers.size();
    // The following will only work if sizeof(enum)==sizeof(int)
    pj_assert(sizeof(ts.ciphers[0]) == sizeof(int));
    ts.ciphers		= (pj_ssl_cipher*)&this->ciphers[0];
    ts.verify_server	= this->verifyServer;
    ts.verify_client	= this->verifyClient;
    ts.require_client_cert = this->requireClientCert;
    ts.timeout.sec 	= this->msecTimeout / 1000;
    ts.timeout.msec	= this->msecTimeout % 1000;
    ts.qos_type		= this->qosType;
    ts.qos_params	= this->qosParams;
    ts.qos_ignore_error	= this->qosIgnoreError;

    return ts;
}

void TlsConfig::fromPj(const pjsip_tls_setting &prm)
{
    this->CaListFile 	= pj2Str(prm.ca_list_file);
    this->certFile 	= pj2Str(prm.cert_file);
    this->privKeyFile 	= pj2Str(prm.privkey_file);
    this->password 	= pj2Str(prm.password);
    this->method 	= (pjsip_ssl_method)prm.method;
    // The following will only work if sizeof(enum)==sizeof(int)
    pj_assert(sizeof(prm.ciphers[0]) == sizeof(int));
    this->ciphers 	= IntVector(prm.ciphers, prm.ciphers+prm.ciphers_num);
    this->verifyServer 	= prm.verify_server;
    this->verifyClient 	= prm.verify_client;
    this->requireClientCert = prm.require_client_cert;
    this->msecTimeout 	= PJ_TIME_VAL_MSEC(prm.timeout);
    this->qosType 	= prm.qos_type;
    this->qosParams 	= prm.qos_params;
    this->qosIgnoreError = prm.qos_ignore_error;
}

void TlsConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("TlsConfig");

    NODE_READ_STRING  ( this_node, CaListFile);
    NODE_READ_STRING  ( this_node, certFile);
    NODE_READ_STRING  ( this_node, privKeyFile);
    NODE_READ_STRING  ( this_node, password);
    NODE_READ_NUM_T   ( this_node, pjsip_ssl_method, method);
    readIntVector     ( this_node, "ciphers", ciphers);
    NODE_READ_BOOL    ( this_node, verifyServer);
    NODE_READ_BOOL    ( this_node, verifyClient);
    NODE_READ_BOOL    ( this_node, requireClientCert);
    NODE_READ_UNSIGNED( this_node, msecTimeout);
    NODE_READ_NUM_T   ( this_node, pj_qos_type, qosType);
    readQosParams     ( this_node, qosParams);
    NODE_READ_BOOL    ( this_node, qosIgnoreError);
}

void TlsConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("TlsConfig");

    NODE_WRITE_STRING  ( this_node, CaListFile);
    NODE_WRITE_STRING  ( this_node, certFile);
    NODE_WRITE_STRING  ( this_node, privKeyFile);
    NODE_WRITE_STRING  ( this_node, password);
    NODE_WRITE_NUM_T   ( this_node, pjsip_ssl_method, method);
    writeIntVector     ( this_node, "ciphers", ciphers);
    NODE_WRITE_BOOL    ( this_node, verifyServer);
    NODE_WRITE_BOOL    ( this_node, verifyClient);
    NODE_WRITE_BOOL    ( this_node, requireClientCert);
    NODE_WRITE_UNSIGNED( this_node, msecTimeout);
    NODE_WRITE_NUM_T   ( this_node, pj_qos_type, qosType);
    writeQosParams     ( this_node, qosParams);
    NODE_WRITE_BOOL    ( this_node, qosIgnoreError);
}

///////////////////////////////////////////////////////////////////////////////

TransportConfig::TransportConfig()
{
    pjsua_transport_config tc;
    pjsua_transport_config_default(&tc);
    this->fromPj(tc);
}

void TransportConfig::fromPj(const pjsua_transport_config &prm)
{
    this->port 		= prm.port;
    this->portRange	= prm.port_range;
    this->publicAddress = pj2Str(prm.public_addr);
    this->boundAddress	= pj2Str(prm.bound_addr);
    this->tlsConfig.fromPj(prm.tls_setting);
    this->qosType	= prm.qos_type;
    this->qosParams	= prm.qos_params;
}

pjsua_transport_config TransportConfig::toPj() const
{
    pjsua_transport_config tc;

    tc.port		= this->port;
    tc.port_range	= this->portRange;
    tc.public_addr	= str2Pj(this->publicAddress);
    tc.bound_addr	= str2Pj(this->boundAddress);
    tc.tls_setting	= this->tlsConfig.toPj();
    tc.qos_type		= this->qosType;
    tc.qos_params	= this->qosParams;

    return tc;
}

void TransportConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("TransportConfig");

    NODE_READ_UNSIGNED  ( this_node, port);
    NODE_READ_UNSIGNED  ( this_node, portRange);
    NODE_READ_STRING    ( this_node, publicAddress);
    NODE_READ_STRING    ( this_node, boundAddress);
    NODE_READ_NUM_T     ( this_node, pj_qos_type, qosType);
    readQosParams       ( this_node, qosParams);
    NODE_READ_OBJ       ( this_node, tlsConfig);
}

void TransportConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("TransportConfig");

    NODE_WRITE_UNSIGNED  ( this_node, port);
    NODE_WRITE_UNSIGNED  ( this_node, portRange);
    NODE_WRITE_STRING    ( this_node, publicAddress);
    NODE_WRITE_STRING    ( this_node, boundAddress);
    NODE_WRITE_NUM_T     ( this_node, pj_qos_type, qosType);
    writeQosParams       ( this_node, qosParams);
    NODE_WRITE_OBJ       ( this_node, tlsConfig);
}

///////////////////////////////////////////////////////////////////////////////

TransportInfo::TransportInfo(const pjsua_transport_info &info)
{
    this->id = info.id;
    this->type = info.type;
    this->typeName = pj2Str(info.type_name);
    this->info = pj2Str(info.info);
    this->flags = info.flag;

    char straddr[PJ_INET6_ADDRSTRLEN+10];
    pj_sockaddr_print(&info.local_addr, straddr, sizeof(straddr), 3);
    this->localAddress = straddr;

    pj_ansi_snprintf(straddr, sizeof(straddr), "%.*s:%d",
                     (int)info.local_name.host.slen,
                     info.local_name.host.ptr,
                     info.local_name.port);
    this->localName = straddr;
    this->usageCount = info.usage_count;
}

///////////////////////////////////////////////////////////////////////////////

void SipRxData::fromPj(pjsip_rx_data &rdata)
{
    info	= pjsip_rx_data_get_info(&rdata);
    wholeMsg	= string(rdata.msg_info.msg_buf, rdata.msg_info.len);
    srcIp	= rdata.pkt_info.src_name;
    srcPort	= rdata.pkt_info.src_port;
}

///////////////////////////////////////////////////////////////////////////////

void SipMediaType::fromPj(const pjsip_media_type &prm)
{
    type	= pj2Str(prm.type);
    subType	= pj2Str(prm.subtype);
}

pjsip_media_type SipMediaType::toPj() const
{
    pjsip_media_type pj_mt;
    pj_bzero(&pj_mt, sizeof(pj_mt));
    pj_mt.type	    = str2Pj(type);
    pj_mt.subtype   = str2Pj(subType);
    return pj_mt;
}

///////////////////////////////////////////////////////////////////////////////

void SipHeader::fromPj(const pjsip_hdr *hdr) throw(Error)
{
    char buf[256];

    int len = pjsip_hdr_print_on((void*)hdr, buf, sizeof(buf)-1);
    if (len <= 0)
	PJSUA2_RAISE_ERROR(PJ_ETOOSMALL);
    buf[len] = '\0';

    char *pos = strchr(buf, ':');
    if (!pos)
	PJSUA2_RAISE_ERROR(PJSIP_EINVALIDHDR);

    // Trim white space after header name
    char *end_name = pos;
    while (end_name>buf && pj_isspace(*(end_name-1))) --end_name;

    // Trim whitespaces after colon
    char *start_val = pos+1;
    while (*start_val && pj_isspace(*start_val)) ++start_val;

    hName = string(buf, end_name);
    hValue = string(start_val);
}

pjsip_generic_string_hdr &SipHeader::toPj() const
{
    pj_str_t hname  = str2Pj(hName);
    pj_str_t hvalue = str2Pj(hValue);

    pjsip_generic_string_hdr_init2(&pjHdr, &hname, &hvalue);
    return pjHdr;
}

///////////////////////////////////////////////////////////////////////////////

void SipMultipartPart::fromPj(const pjsip_multipart_part &prm) throw(Error)
{
    headers.clear();
    pjsip_hdr* pj_hdr = prm.hdr.next;
    while (pj_hdr != &prm.hdr) {
	SipHeader sh;
	sh.fromPj(pj_hdr);
	headers.push_back(sh);
	pj_hdr = pj_hdr->next;
    }

    if (!prm.body)
	PJSUA2_RAISE_ERROR(PJ_EINVAL);
    
    contentType.fromPj(prm.body->content_type);
    body = string((char*)prm.body->data, prm.body->len);
}

pjsip_multipart_part& SipMultipartPart::toPj() const
{
    pj_list_init(&pjMpp.hdr);
    for (unsigned i = 0; i < headers.size(); i++) {
	pjsip_generic_string_hdr& pj_hdr = headers[i].toPj();
	pj_list_push_back(&pjMpp.hdr, &pj_hdr);
    }

    pj_bzero(&pjMsgBody, sizeof(pjMsgBody));
    pjMsgBody.content_type  = contentType.toPj();
    pjMsgBody.print_body    = &pjsip_print_text_body;
    pjMsgBody.clone_data    = &pjsip_clone_text_data;
    pjMsgBody.data	    = (void*)body.c_str();
    pjMsgBody.len	    = body.size();
    pjMpp.body = &pjMsgBody;

    return pjMpp;
}

///////////////////////////////////////////////////////////////////////////////

void SipTxOption::fromPj(const pjsua_msg_data &prm) throw(Error)
{
    targetUri = pj2Str(prm.target_uri);

    headers.clear();
    pjsip_hdr* pj_hdr = prm.hdr_list.next;
    while (pj_hdr != &prm.hdr_list) {
	SipHeader sh;
	sh.fromPj(pj_hdr);
	headers.push_back(sh);
	pj_hdr = pj_hdr->next;
    }

    contentType = pj2Str(prm.content_type);
    msgBody = pj2Str(prm.msg_body);
    multipartContentType.fromPj(prm.multipart_ctype);

    multipartParts.clear();
    pjsip_multipart_part* pj_mp = prm.multipart_parts.next;
    while (pj_mp != &prm.multipart_parts) {
	SipMultipartPart smp;
	smp.fromPj(*pj_mp);
	multipartParts.push_back(smp);
	pj_mp = pj_mp->next;
    }
}

void SipTxOption::toPj(pjsua_msg_data &msg_data) const
{
    unsigned i;

    pjsua_msg_data_init(&msg_data);

    msg_data.target_uri = str2Pj(targetUri);

    pj_list_init(&msg_data.hdr_list);
    for (i = 0; i < headers.size(); i++) {
	pjsip_generic_string_hdr& pj_hdr = headers[i].toPj();
	pj_list_push_back(&msg_data.hdr_list, &pj_hdr);
    }

    msg_data.content_type = str2Pj(contentType);
    msg_data.msg_body = str2Pj(msgBody);
    msg_data.multipart_ctype = multipartContentType.toPj();

    pj_list_init(&msg_data.multipart_parts);
    for (i = 0; i < multipartParts.size(); i++) {
	pjsip_multipart_part& pj_part = multipartParts[i].toPj();
	pj_list_push_back(&msg_data.multipart_parts, &pj_part);
    }
}
