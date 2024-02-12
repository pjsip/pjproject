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

#define THIS_FILE       "siptypes.cpp"

///////////////////////////////////////////////////////////////////////////////
namespace pj
{
void readIntVector( ContainerNode &node,
                    const string &array_name,
                    IntVector &v) PJSUA2_THROW(Error)
{
    ContainerNode array_node = node.readArray(array_name);
    v.resize(0);
    while (array_node.hasUnread()) {
        v.push_back((int)array_node.readNumber());
    }
}

void writeIntVector(ContainerNode &node,
                    const string &array_name,
                    const IntVector &v) PJSUA2_THROW(Error)
{
    ContainerNode array_node = node.writeNewArray(array_name);
    for (unsigned i=0; i<v.size(); ++i) {
        array_node.writeNumber("", (float)v[i]);
    }
}

void readQosParams( ContainerNode &node,
                    pj_qos_params &qos) PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.readContainer("qosParams");

    NODE_READ_NUM_T( this_node, pj_uint8_t, qos.flags);
    NODE_READ_NUM_T( this_node, pj_uint8_t, qos.dscp_val);
    NODE_READ_NUM_T( this_node, pj_uint8_t, qos.so_prio);
    NODE_READ_NUM_T( this_node, pj_qos_wmm_prio, qos.wmm_prio);
}

void writeQosParams( ContainerNode &node,
                     const pj_qos_params &qos) PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.writeNewContainer("qosParams");

    NODE_WRITE_NUM_T( this_node, pj_uint8_t, qos.flags);
    NODE_WRITE_NUM_T( this_node, pj_uint8_t, qos.dscp_val);
    NODE_WRITE_NUM_T( this_node, pj_uint8_t, qos.so_prio);
    NODE_WRITE_NUM_T( this_node, pj_qos_wmm_prio, qos.wmm_prio);
}

void readSipHeaders( const ContainerNode &node,
                     const string &array_name,
                     SipHeaderVector &headers) PJSUA2_THROW(Error)
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
                     const SipHeaderVector &headers) PJSUA2_THROW(Error)
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

void AuthCredInfo::readObject(const ContainerNode &node) PJSUA2_THROW(Error)
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

void AuthCredInfo::writeObject(ContainerNode &node) const PJSUA2_THROW(Error)
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

void AuthCredInfo::fromPj(const pjsip_cred_info &prm)
{
    realm       = pj2Str(prm.realm);
    scheme      = pj2Str(prm.scheme);
    username    = pj2Str(prm.username);
    dataType    = prm.data_type;
    data        = pj2Str(prm.data);
    akaK        = pj2Str(prm.ext.aka.k);
    akaOp       = pj2Str(prm.ext.aka.op);
    akaAmf      = pj2Str(prm.ext.aka.amf);
}

pjsip_cred_info AuthCredInfo::toPj() const
{
    pjsip_cred_info ret;
    ret.realm   = str2Pj(realm);
    ret.scheme  = str2Pj(scheme);
    ret.username        = str2Pj(username);
    ret.data_type       = dataType;
    ret.data    = str2Pj(data);
    ret.ext.aka.k       = str2Pj(akaK);
    ret.ext.aka.op      = str2Pj(akaOp);
    ret.ext.aka.amf     = str2Pj(akaAmf);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////

TlsConfig::TlsConfig() : method(PJSIP_SSL_UNSPECIFIED_METHOD),
                         qosType(PJ_QOS_TYPE_BEST_EFFORT)
{
    pjsip_tls_setting ts;
    pjsip_tls_setting_default(&ts);
    this->fromPj(ts);
}

pjsip_tls_setting TlsConfig::toPj() const
{
    pjsip_tls_setting ts;
    pjsip_tls_setting_default(&ts);

    ts.ca_list_file     = str2Pj(this->CaListFile);
    ts.cert_file        = str2Pj(this->certFile);
    ts.privkey_file     = str2Pj(this->privKeyFile);
    ts.password         = str2Pj(this->password);
    ts.ca_buf           = str2Pj(this->CaBuf);
    ts.cert_buf         = str2Pj(this->certBuf);
    ts.privkey_buf      = str2Pj(this->privKeyBuf);
    ts.method           = this->method;
    ts.ciphers_num      = (unsigned)this->ciphers.size();
    ts.proto            = this->proto;
    // The following will only work if sizeof(enum)==sizeof(int)
    pj_assert(sizeof(ts.ciphers[0]) == sizeof(int));
    ts.ciphers          = ts.ciphers_num? 
                            (pj_ssl_cipher*)&this->ciphers[0] : NULL;
    ts.verify_server    = this->verifyServer;
    ts.verify_client    = this->verifyClient;
    ts.require_client_cert = this->requireClientCert;
    ts.timeout.sec      = this->msecTimeout / 1000;
    ts.timeout.msec     = this->msecTimeout % 1000;
    ts.qos_type         = this->qosType;
    ts.qos_params       = this->qosParams;
    ts.qos_ignore_error = this->qosIgnoreError;
    ts.enable_renegotiation = this->enableRenegotiation;

    return ts;
}

void TlsConfig::fromPj(const pjsip_tls_setting &prm)
{
    this->CaListFile    = pj2Str(prm.ca_list_file);
    this->certFile      = pj2Str(prm.cert_file);
    this->privKeyFile   = pj2Str(prm.privkey_file);
    this->password      = pj2Str(prm.password);
    this->CaBuf         = pj2Str(prm.ca_buf);
    this->certBuf       = pj2Str(prm.cert_buf);
    this->privKeyBuf    = pj2Str(prm.privkey_buf);
    this->method        = (pjsip_ssl_method)prm.method;
    this->proto         = prm.proto;
    // The following will only work if sizeof(enum)==sizeof(int)
    pj_assert(sizeof(prm.ciphers[0]) == sizeof(int));
    this->ciphers       = IntVector(prm.ciphers, prm.ciphers+prm.ciphers_num);
    this->verifyServer  = PJ2BOOL(prm.verify_server);
    this->verifyClient  = PJ2BOOL(prm.verify_client);
    this->requireClientCert = PJ2BOOL(prm.require_client_cert);
    this->msecTimeout   = PJ_TIME_VAL_MSEC(prm.timeout);
    this->qosType       = prm.qos_type;
    this->qosParams     = prm.qos_params;
    this->qosIgnoreError = PJ2BOOL(prm.qos_ignore_error);
    this->enableRenegotiation = PJ2BOOL(prm.enable_renegotiation);
}

void TlsConfig::readObject(const ContainerNode &node) PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.readContainer("TlsConfig");

    NODE_READ_STRING  ( this_node, CaListFile);
    NODE_READ_STRING  ( this_node, certFile);
    NODE_READ_STRING  ( this_node, privKeyFile);
    NODE_READ_STRING  ( this_node, password);
    NODE_READ_STRING  ( this_node, CaBuf);
    NODE_READ_STRING  ( this_node, certBuf);
    NODE_READ_STRING  ( this_node, privKeyBuf);
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

void TlsConfig::writeObject(ContainerNode &node) const PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.writeNewContainer("TlsConfig");

    NODE_WRITE_STRING  ( this_node, CaListFile);
    NODE_WRITE_STRING  ( this_node, certFile);
    NODE_WRITE_STRING  ( this_node, privKeyFile);
    NODE_WRITE_STRING  ( this_node, password);
    NODE_WRITE_STRING  ( this_node, CaBuf);
    NODE_WRITE_STRING  ( this_node, certBuf);
    NODE_WRITE_STRING  ( this_node, privKeyBuf);
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

TransportConfig::TransportConfig() : qosType(PJ_QOS_TYPE_BEST_EFFORT)
{
    pjsua_transport_config tc;
    pjsua_transport_config_default(&tc);
    this->fromPj(tc);
}

void TransportConfig::fromPj(const pjsua_transport_config &prm)
{
    this->port          = prm.port;
    this->portRange     = prm.port_range;
    this->randomizePort = PJ2BOOL(prm.randomize_port);
    this->publicAddress = pj2Str(prm.public_addr);
    this->boundAddress  = pj2Str(prm.bound_addr);
    this->tlsConfig.fromPj(prm.tls_setting);
    this->qosType       = prm.qos_type;
    this->qosParams     = prm.qos_params;
}

pjsua_transport_config TransportConfig::toPj() const
{
    pjsua_transport_config tc;
    pjsua_transport_config_default(&tc);

    tc.port             = this->port;
    tc.port_range       = this->portRange;
    tc.randomize_port   = this->randomizePort;
    tc.public_addr      = str2Pj(this->publicAddress);
    tc.bound_addr       = str2Pj(this->boundAddress);
    tc.tls_setting      = this->tlsConfig.toPj();
    tc.qos_type         = this->qosType;
    tc.qos_params       = this->qosParams;

    return tc;
}

void TransportConfig::readObject(const ContainerNode &node) PJSUA2_THROW(Error)
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

void TransportConfig::writeObject(ContainerNode &node) const
                                  PJSUA2_THROW(Error)
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

TransportInfo::TransportInfo()
: id(), type(PJSIP_TRANSPORT_UNSPECIFIED), flags(), usageCount()
{
}

void TransportInfo::fromPj(const pjsua_transport_info &tinfo)
{
    this->id = tinfo.id;
    this->type = tinfo.type;
    this->typeName = pj2Str(tinfo.type_name);
    this->info = pj2Str(tinfo.info);
    this->flags = tinfo.flag;

    char straddr[PJ_INET6_ADDRSTRLEN+10];
    pj_sockaddr_print(&tinfo.local_addr, straddr, sizeof(straddr), 3);
    this->localAddress = straddr;

    pj_ansi_snprintf(straddr, sizeof(straddr), "%.*s:%d",
                     (int)tinfo.local_name.host.slen,
                     tinfo.local_name.host.ptr,
                     tinfo.local_name.port);
    this->localName = straddr;
    this->usageCount = tinfo.usage_count;
}

///////////////////////////////////////////////////////////////////////////////

SipRxData::SipRxData()
: pjRxData(NULL)
{
}

void SipRxData::fromPj(pjsip_rx_data &rdata)
{
    char straddr[PJ_INET6_ADDRSTRLEN+10];

    info        = pjsip_rx_data_get_info(&rdata);
    wholeMsg    = string(rdata.msg_info.msg_buf, rdata.msg_info.len);
    pj_sockaddr_print(&rdata.pkt_info.src_addr, straddr, sizeof(straddr), 3);
    srcAddress  = straddr;
    pjRxData    = (void *)&rdata;
}

///////////////////////////////////////////////////////////////////////////////

void SipMediaType::fromPj(const pjsip_media_type &prm)
{
    type        = pj2Str(prm.type);
    subType     = pj2Str(prm.subtype);
}

pjsip_media_type SipMediaType::toPj() const
{
    pjsip_media_type pj_mt;
    pj_bzero(&pj_mt, sizeof(pj_mt));
    pj_mt.type      = str2Pj(type);
    pj_mt.subtype   = str2Pj(subType);
    return pj_mt;
}

///////////////////////////////////////////////////////////////////////////////

SipHeader::SipHeader()
{
    pj_str_t dummy;

    // Init pjHdr with null string to suppress warning
    pj_bzero(&dummy, sizeof(dummy));
    pjsip_generic_string_hdr_init2(&pjHdr, &dummy, &dummy);
}

void SipHeader::fromPj(const pjsip_hdr *hdr) PJSUA2_THROW(Error)
{
    char *buf = NULL;
    int len = 0;
    unsigned buf_size = 256>>1;

    /* Print header to a 256 bytes buffer first.
     * If buffer is not sufficient, try 512, 1024, soon
     * until > PJSIP_MAX_PKT_LEN
     */
    do {
        buf_size <<= 1;
        buf = (char*)malloc(buf_size);
        if (!buf)
            PJSUA2_RAISE_ERROR(PJ_ENOMEM);

        len = pjsip_hdr_print_on((void*)hdr, buf, buf_size-1);
        if (len < 0)
            free(buf);

    } while ((buf_size < PJSIP_MAX_PKT_LEN) && (len < 0));
    
    if (len < 0)
        PJSUA2_RAISE_ERROR(PJ_ETOOSMALL);

    buf[len] = '\0';

    char *pos = strchr(buf, ':');
    if (!pos) {
        free(buf);
        PJSUA2_RAISE_ERROR(PJSIP_EINVALIDHDR);
    }

    // Trim white space after header name
    char *end_name = pos;
    while (end_name>buf && pj_isspace(*(end_name-1))) --end_name;

    // Trim whitespaces after colon
    char *start_val = pos+1;
    while (*start_val && pj_isspace(*start_val)) ++start_val;

    hName = string(buf, end_name);
    hValue = string(start_val);
    free(buf);
}

pjsip_generic_string_hdr &SipHeader::toPj() const
{
    pj_str_t hname  = str2Pj(hName);
    pj_str_t hvalue = str2Pj(hValue);

    pjsip_generic_string_hdr_init2(&pjHdr, &hname, &hvalue);
    return pjHdr;
}

///////////////////////////////////////////////////////////////////////////////

SipMultipartPart::SipMultipartPart()
{
    pj_bzero(&pjMpp, sizeof(pjMpp));
    pj_bzero(&pjMsgBody, sizeof(pjMsgBody));
    pj_list_init(&pjMpp.hdr);
}

void SipMultipartPart::fromPj(const pjsip_multipart_part &prm)
                              PJSUA2_THROW(Error)
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

    pj_list_init(&pjMpp.hdr);
    pjMpp.body = NULL;
    pj_bzero(&pjMsgBody, sizeof(pjMsgBody));
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
    pjMsgBody.data          = (void*)body.c_str();
    pjMsgBody.len           = (unsigned)body.size();
    pjMpp.body = &pjMsgBody;

    return pjMpp;
}

///////////////////////////////////////////////////////////////////////////////

SipEvent::SipEvent()
: type(PJSIP_EVENT_UNKNOWN), pjEvent(NULL)
{
}

void SipEvent::fromPj(const pjsip_event &ev)
{
    type = ev.type;
    if (type == PJSIP_EVENT_TIMER) {
        body.timer.entry = ev.body.timer.entry;
    } else if (type == PJSIP_EVENT_TSX_STATE) {
        body.tsxState.prevState = (pjsip_tsx_state_e)
        ev.body.tsx_state.prev_state;
        body.tsxState.tsx.fromPj(*ev.body.tsx_state.tsx);
        body.tsxState.type = ev.body.tsx_state.type;
        if (body.tsxState.type == PJSIP_EVENT_TX_MSG) {
            if (ev.body.tsx_state.src.tdata)
                body.tsxState.src.tdata.fromPj(*ev.body.tsx_state.src.tdata);
        } else if (body.tsxState.type == PJSIP_EVENT_RX_MSG) {
            if (ev.body.tsx_state.src.rdata)
                body.tsxState.src.rdata.fromPj(*ev.body.tsx_state.src.rdata);
        } else if (body.tsxState.type == PJSIP_EVENT_TRANSPORT_ERROR) {
            body.tsxState.src.status = ev.body.tsx_state.src.status;
        } else if (body.tsxState.type == PJSIP_EVENT_TIMER) {
            body.tsxState.src.timer = ev.body.tsx_state.src.timer;
        } else if (body.tsxState.type == PJSIP_EVENT_USER) {
            body.tsxState.src.data = ev.body.tsx_state.src.data;
        }
    } else if (type == PJSIP_EVENT_TX_MSG) {
        if (ev.body.tx_msg.tdata)
            body.txMsg.tdata.fromPj(*ev.body.tx_msg.tdata);
    } else if (type == PJSIP_EVENT_RX_MSG) {
        if (ev.body.rx_msg.rdata)
            body.rxMsg.rdata.fromPj(*ev.body.rx_msg.rdata);
    } else if (type == PJSIP_EVENT_TRANSPORT_ERROR) {
        if (ev.body.tx_error.tdata)
            body.txError.tdata.fromPj(*ev.body.tx_error.tdata);
        if (ev.body.tx_error.tsx)
            body.txError.tsx.fromPj(*ev.body.tx_error.tsx);
    } else if (type == PJSIP_EVENT_USER) {
        body.user.user1 = ev.body.user.user1;
        body.user.user2 = ev.body.user.user2;
        body.user.user3 = ev.body.user.user3;
        body.user.user4 = ev.body.user.user4;
    }
    pjEvent = (void *)&ev;
}

SipTxData::SipTxData()
: pjTxData(NULL)
{
}

void SipTxData::fromPj(pjsip_tx_data &tdata)
{
    char straddr[PJ_INET6_ADDRSTRLEN+10];
    
    info        = pjsip_tx_data_get_info(&tdata);
    pjsip_tx_data_encode(&tdata);
    wholeMsg    = string(tdata.buf.start, tdata.buf.cur - tdata.buf.start);
    if (pj_sockaddr_has_addr(&tdata.tp_info.dst_addr)) {
        pj_sockaddr_print(&tdata.tp_info.dst_addr, straddr, sizeof(straddr), 3);
        dstAddress  = straddr;
    } else {
        dstAddress = "";
    }
    pjTxData    = (void *)&tdata;
}

SipTransaction::SipTransaction()
: role(PJSIP_ROLE_UAC), statusCode(PJSIP_SC_NULL),
  state(PJSIP_TSX_STATE_NULL), pjTransaction(NULL)
{
}

void SipTransaction::fromPj(pjsip_transaction &tsx)
{
    this->role          = tsx.role;
    this->method        = pj2Str(tsx.method.name);
    this->statusCode    = tsx.status_code;
    this->statusText    = pj2Str(tsx.status_text);
    this->state         = tsx.state;
    if (tsx.last_tx)
        this->lastTx.fromPj(*tsx.last_tx);
    else
        this->lastTx.pjTxData = NULL;
    this->pjTransaction = (void *)&tsx;
}

TsxStateEvent::TsxStateEvent()
: prevState(PJSIP_TSX_STATE_NULL), type(PJSIP_EVENT_UNKNOWN)
{
}

bool SipTxOption::isEmpty() const
{
    return (targetUri == "" && localUri == "" &&  headers.size() == 0 &&
            contentType == "" && msgBody == "" && multipartContentType.type == "" &&
            multipartContentType.subType == "" && multipartParts.size() == 0);
}

void SipTxOption::fromPj(const pjsua_msg_data &prm) PJSUA2_THROW(Error)
{
    targetUri = pj2Str(prm.target_uri);

    localUri = pj2Str(prm.local_uri);

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

    msg_data.local_uri = str2Pj(localUri);

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

//////////////////////////////////////////////////////////////////////////////

SendInstantMessageParam::SendInstantMessageParam()
: contentType("text/plain"), content(""), userData(NULL)
{
}

SendTypingIndicationParam::SendTypingIndicationParam()
: isTyping(false)
{
}
