/* $Id$ */
/*
 * Copyright (C) 2012 Teluu Inc. (http://www.teluu.com)
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
#include "util.hpp"

using namespace pj;
using namespace std;

#define THIS_FILE	"types.cpp"

///////////////////////////////////////////////////////////////////////////////

Error::Error()
: status(PJ_SUCCESS), srcLine(0)
{
}

Error::Error( pj_status_t prm_status,
	      const string &prm_title,
	      const string &prm_reason,
	      const string &prm_src_file,
	      int prm_src_line)
: status(prm_status), title(prm_title), reason(prm_reason),
  srcFile(prm_src_file), srcLine(prm_src_line)
{
    if (this->status != PJ_SUCCESS && prm_reason.empty()) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(this->status, errmsg, sizeof(errmsg));
	this->reason = errmsg;
    }
}

string Error::info(bool multi_line) const
{
    string output;

    if (status==PJ_SUCCESS) {
	output = "No error";
    } else if (!multi_line) {
	char temp[80];

	if (!title.empty()) {
	    output += title + " error: ";
	}
	snprintf(temp, sizeof(temp), " (status=%d)", status);
	output += reason + temp;
	if (!srcFile.empty()) {
	    output += " [";
	    output += srcFile;
	    snprintf(temp, sizeof(temp), ":%d]", srcLine);
	    output += temp;
	}
    } else {
	char temp[80];

	if (!title.empty()) {
	    output += string("Title:       ") + title + "\n";
	}

	snprintf(temp, sizeof(temp), "%d\n", status);
	output += string("Code:        ") + temp;
	output += string("Description: ") + reason + "\n";
	if (!srcFile.empty()) {
	    snprintf(temp, sizeof(temp), ":%d\n", srcLine);
	    output += string("Location:    ") + srcFile + temp;
	}
    }

    return output;
}

///////////////////////////////////////////////////////////////////////////////

AuthCredInfo::AuthCredInfo()
: dataType(0)
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
