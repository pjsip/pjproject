/* $Id$ */
/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_SSL_SOCK_H__
#define __PJ_SSL_SOCK_H__

/**
 * @file ssl_sock.h
 * @brief Secure socket
 */

#include <pj/ioqueue.h>
#include <pj/sock.h>
#include <pj/sock_qos.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJ_SSL_SOCK Secure socket I/O
 * @brief Secure socket provides security on socket operation using standard
 * security protocols such as SSL and TLS.
 * @ingroup PJ_IO
 * @{
 *
 * Secure socket wraps normal socket and applies security features, i.e: 
 * privacy and data integrity, on the socket traffic, using standard security
 * protocols such as SSL and TLS.
 *
 * Secure socket employs active socket operations, which is similar to (and
 * described more detail) in \ref PJ_ACTIVESOCK.
 */


 /**
 * This opaque structure describes the secure socket.
 */
typedef struct pj_ssl_sock_t pj_ssl_sock_t;


/**
 * Opaque declaration of endpoint certificate or credentials. This may contains
 * certificate, private key, and trusted Certificate Authorities list.
 */
typedef struct pj_ssl_cert_t pj_ssl_cert_t;


typedef enum pj_ssl_cert_verify_flag_t
{
    /**
     * No error in verification.
     */
    PJ_SSL_CERT_ESUCCESS				= 0,

    /**
     * The issuer certificate cannot be found.
     */
    PJ_SSL_CERT_EISSUER_NOT_FOUND			= (1 << 0),

    /**
     * The certificate is untrusted.
     */
    PJ_SSL_CERT_EUNTRUSTED				= (1 << 1),

    /**
     * The certificate has expired or not yet valid.
     */
    PJ_SSL_CERT_EVALIDITY_PERIOD			= (1 << 2),

    /**
     * One or more fields of the certificate cannot be decoded due to
     * invalid format.
     */
    PJ_SSL_CERT_EINVALID_FORMAT				= (1 << 3),

    /**
     * The certificate cannot be used for the specified purpose.
     */
    PJ_SSL_CERT_EINVALID_PURPOSE			= (1 << 4),

    /**
     * The issuer info in the certificate does not match to the (candidate) 
     * issuer certificate, e.g: issuer name not match to subject name
     * of (candidate) issuer certificate.
     */
    PJ_SSL_CERT_EISSUER_MISMATCH			= (1 << 5),

    /**
     * The CRL certificate cannot be found or cannot be read properly.
     */
    PJ_SSL_CERT_ECRL_FAILURE				= (1 << 6),

    /**
     * The certificate has been revoked.
     */
    PJ_SSL_CERT_EREVOKED				= (1 << 7),

    /**
     * The certificate chain length is too long.
     */
    PJ_SSL_CERT_ECHAIN_TOO_LONG				= (1 << 8),

    /**
     * The server identity does not match to any identities specified in 
     * the certificate, e.g: subjectAltName extension, subject common name.
     * This flag will only be set by application as SSL socket does not 
     * perform server identity verification.
     */
    PJ_SSL_CERT_EIDENTITY_NOT_MATCH			= (1 << 30),

    /**
     * Unknown verification error.
     */
    PJ_SSL_CERT_EUNKNOWN				= (1 << 31)

} pj_ssl_cert_verify_flag_t;


typedef enum pj_ssl_cert_name_type
{
    PJ_SSL_CERT_NAME_UNKNOWN = 0,
    PJ_SSL_CERT_NAME_RFC822,
    PJ_SSL_CERT_NAME_DNS,
    PJ_SSL_CERT_NAME_URI,
    PJ_SSL_CERT_NAME_IP
} pj_ssl_cert_name_type;

/**
 * Describe structure of certificate info.
 */
typedef struct pj_ssl_cert_info {

    unsigned	version;	    /**< Certificate version	*/

    pj_uint8_t	serial_no[20];	    /**< Serial number, array of
				         octets, first index is
					 MSB			*/

    struct {
        pj_str_t	cn;	    /**< Common name		*/
        pj_str_t	info;	    /**< One line subject, fields
					 are separated by slash, e.g:
					 "CN=sample.org/OU=HRD" */
    } subject;			    /**< Subject		*/

    struct {
        pj_str_t	cn;	    /**< Common name		*/
        pj_str_t	info;	    /**< One line subject, fields
					 are separated by slash.*/
    } issuer;			    /**< Issuer			*/

    struct {
	pj_time_val	start;	    /**< Validity start		*/
	pj_time_val	end;	    /**< Validity end		*/
	pj_bool_t	gmt;	    /**< Flag if validity date/time 
					 use GMT		*/
    } validity;			    /**< Validity		*/

    struct {
	unsigned	cnt;	    /**< # of entry		*/
	struct {
	    pj_ssl_cert_name_type type;
				    /**< Name type		*/
	    pj_str_t	name;	    /**< The name		*/
	} *entry;		    /**< Subject alt name entry */
    } subj_alt_name;		    /**< Subject alternative
					 name extension		*/

    pj_str_t raw;		    /**< Raw certificate in PEM format, only
					 available for remote certificate. */

    struct {
        unsigned    	cnt;        /**< # of entry     */
        pj_str_t       *cert_raw;
    } raw_chain;

} pj_ssl_cert_info;

/**
 * The SSL certificate buffer.
 */
typedef pj_str_t pj_ssl_cert_buffer;

/**
 * Create credential from files. TLS server application can provide multiple
 * certificates (RSA, ECC, and DSA) by supplying certificate name with "_rsa"
 * suffix, e.g: "pjsip_rsa.pem", the library will automatically check for
 * other certificates with "_ecc" and "_dsa" suffix.
 *
 * @param CA_file	The file of trusted CA list.
 * @param cert_file	The file of certificate.
 * @param privkey_file	The file of private key.
 * @param privkey_pass	The password of private key, if any.
 * @param p_cert	Pointer to credential instance to be created.
 *
 * @return		PJ_SUCCESS when successful.
 */
PJ_DECL(pj_status_t) pj_ssl_cert_load_from_files(pj_pool_t *pool,
						 const pj_str_t *CA_file,
						 const pj_str_t *cert_file,
						 const pj_str_t *privkey_file,
						 const pj_str_t *privkey_pass,
						 pj_ssl_cert_t **p_cert);

/**
 * Create credential from files. TLS server application can provide multiple
 * certificates (RSA, ECC, and DSA) by supplying certificate name with "_rsa"
 * suffix, e.g: "pjsip_rsa.pem", the library will automatically check for
 * other certificates with "_ecc" and "_dsa" suffix.
 *
 * This is the same as pj_ssl_cert_load_from_files() but also
 * accepts an additional param CA_path to load CA certificates from
 * a directory.
 *
 * @param CA_file	The file of trusted CA list.
 * @param CA_path	The path to a directory of trusted CA list.
 * @param cert_file	The file of certificate.
 * @param privkey_file	The file of private key.
 * @param privkey_pass	The password of private key, if any.
 * @param p_cert	Pointer to credential instance to be created.
 *
 * @return		PJ_SUCCESS when successful.
 */
PJ_DECL(pj_status_t) pj_ssl_cert_load_from_files2(
						pj_pool_t *pool,
						const pj_str_t *CA_file,
						const pj_str_t *CA_path,
						const pj_str_t *cert_file,
						const pj_str_t *privkey_file,
						const pj_str_t *privkey_pass,
						pj_ssl_cert_t **p_cert);


/**
 * Create credential from data buffer. The certificate expected is in 
 * PEM format.
 *
 * @param CA_buf	The buffer of trusted CA list.
 * @param cert_buf	The buffer of certificate.
 * @param privkey_buf	The buffer of private key.
 * @param privkey_pass	The password of private key, if any.
 * @param p_cert	Pointer to credential instance to be created.
 *
 * @return		PJ_SUCCESS when successful.
 */
PJ_DECL(pj_status_t) pj_ssl_cert_load_from_buffer(pj_pool_t *pool,
					const pj_ssl_cert_buffer *CA_buf,
					const pj_ssl_cert_buffer *cert_buf,
					const pj_ssl_cert_buffer *privkey_buf,
					const pj_str_t *privkey_pass,
					pj_ssl_cert_t **p_cert);

/**
 * Dump SSL certificate info.
 *
 * @param ci		The certificate info.
 * @param indent	String for left indentation.
 * @param buf		The buffer where certificate info will be printed on.
 * @param buf_size	The buffer size.
 *
 * @return		The length of the dump result, or -1 when buffer size
 *			is not sufficient.
 */
PJ_DECL(pj_ssize_t) pj_ssl_cert_info_dump(const pj_ssl_cert_info *ci,
					  const char *indent,
					  char *buf,
					  pj_size_t buf_size);


/**
 * Get SSL certificate verification error messages from verification status.
 *
 * @param verify_status	The SSL certificate verification status.
 * @param error_strings	Array of strings to receive the verification error 
 *			messages.
 * @param count		On input it specifies maximum error messages should be
 *			retrieved. On output it specifies the number of error
 *			messages retrieved.
 *
 * @return		PJ_SUCCESS when successful.
 */
PJ_DECL(pj_status_t) pj_ssl_cert_get_verify_status_strings(
						 pj_uint32_t verify_status, 
						 const char *error_strings[],
						 unsigned *count);

/** 
 * Wipe out the keys in the SSL certificate. 
 *
 * @param cert		The SSL certificate. 
 *
 */
PJ_DECL(void) pj_ssl_cert_wipe_keys(pj_ssl_cert_t *cert);


/** 
 * Cipher suites enumeration.
 */
typedef enum pj_ssl_cipher {

    /* Unsupported cipher */
    PJ_TLS_UNKNOWN_CIPHER                       = -1,

    /* NULL */
    PJ_TLS_NULL_WITH_NULL_NULL               	= 0x00000000,

    /* TLS/SSLv3 */
    PJ_TLS_RSA_WITH_NULL_MD5                 	= 0x00000001,
    PJ_TLS_RSA_WITH_NULL_SHA                 	= 0x00000002,
    PJ_TLS_RSA_WITH_NULL_SHA256              	= 0x0000003B,
    PJ_TLS_RSA_WITH_RC4_128_MD5              	= 0x00000004,
    PJ_TLS_RSA_WITH_RC4_128_SHA              	= 0x00000005,
    PJ_TLS_RSA_WITH_3DES_EDE_CBC_SHA         	= 0x0000000A,
    PJ_TLS_RSA_WITH_AES_128_CBC_SHA          	= 0x0000002F,
    PJ_TLS_RSA_WITH_AES_256_CBC_SHA          	= 0x00000035,
    PJ_TLS_RSA_WITH_AES_128_CBC_SHA256       	= 0x0000003C,
    PJ_TLS_RSA_WITH_AES_256_CBC_SHA256       	= 0x0000003D,
    PJ_TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA      	= 0x0000000D,
    PJ_TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA      	= 0x00000010,
    PJ_TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA     	= 0x00000013,
    PJ_TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA     	= 0x00000016,
    PJ_TLS_DH_DSS_WITH_AES_128_CBC_SHA       	= 0x00000030,
    PJ_TLS_DH_RSA_WITH_AES_128_CBC_SHA       	= 0x00000031,
    PJ_TLS_DHE_DSS_WITH_AES_128_CBC_SHA      	= 0x00000032,
    PJ_TLS_DHE_RSA_WITH_AES_128_CBC_SHA      	= 0x00000033,
    PJ_TLS_DH_DSS_WITH_AES_256_CBC_SHA       	= 0x00000036,
    PJ_TLS_DH_RSA_WITH_AES_256_CBC_SHA       	= 0x00000037,
    PJ_TLS_DHE_DSS_WITH_AES_256_CBC_SHA      	= 0x00000038,
    PJ_TLS_DHE_RSA_WITH_AES_256_CBC_SHA      	= 0x00000039,
    PJ_TLS_DH_DSS_WITH_AES_128_CBC_SHA256    	= 0x0000003E,
    PJ_TLS_DH_RSA_WITH_AES_128_CBC_SHA256    	= 0x0000003F,
    PJ_TLS_DHE_DSS_WITH_AES_128_CBC_SHA256   	= 0x00000040,
    PJ_TLS_DHE_RSA_WITH_AES_128_CBC_SHA256   	= 0x00000067,
    PJ_TLS_DH_DSS_WITH_AES_256_CBC_SHA256    	= 0x00000068,
    PJ_TLS_DH_RSA_WITH_AES_256_CBC_SHA256    	= 0x00000069,
    PJ_TLS_DHE_DSS_WITH_AES_256_CBC_SHA256   	= 0x0000006A,
    PJ_TLS_DHE_RSA_WITH_AES_256_CBC_SHA256   	= 0x0000006B,
    PJ_TLS_DH_anon_WITH_RC4_128_MD5          	= 0x00000018,
    PJ_TLS_DH_anon_WITH_3DES_EDE_CBC_SHA     	= 0x0000001B,
    PJ_TLS_DH_anon_WITH_AES_128_CBC_SHA      	= 0x00000034,
    PJ_TLS_DH_anon_WITH_AES_256_CBC_SHA      	= 0x0000003A,
    PJ_TLS_DH_anon_WITH_AES_128_CBC_SHA256   	= 0x0000006C,
    PJ_TLS_DH_anon_WITH_AES_256_CBC_SHA256   	= 0x0000006D,

    /* TLS (deprecated) */
    PJ_TLS_RSA_EXPORT_WITH_RC4_40_MD5        	= 0x00000003,
    PJ_TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5    	= 0x00000006,
    PJ_TLS_RSA_WITH_IDEA_CBC_SHA             	= 0x00000007,
    PJ_TLS_RSA_EXPORT_WITH_DES40_CBC_SHA     	= 0x00000008,
    PJ_TLS_RSA_WITH_DES_CBC_SHA              	= 0x00000009,
    PJ_TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA  	= 0x0000000B,
    PJ_TLS_DH_DSS_WITH_DES_CBC_SHA           	= 0x0000000C,
    PJ_TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA  	= 0x0000000E,
    PJ_TLS_DH_RSA_WITH_DES_CBC_SHA           	= 0x0000000F,
    PJ_TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA 	= 0x00000011,
    PJ_TLS_DHE_DSS_WITH_DES_CBC_SHA          	= 0x00000012,
    PJ_TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA 	= 0x00000014,
    PJ_TLS_DHE_RSA_WITH_DES_CBC_SHA          	= 0x00000015,
    PJ_TLS_DH_anon_EXPORT_WITH_RC4_40_MD5    	= 0x00000017,
    PJ_TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA 	= 0x00000019,
    PJ_TLS_DH_anon_WITH_DES_CBC_SHA          	= 0x0000001A,

    /* SSLv3 */
    PJ_SSL_FORTEZZA_KEA_WITH_NULL_SHA        	= 0x0000001C,
    PJ_SSL_FORTEZZA_KEA_WITH_FORTEZZA_CBC_SHA	= 0x0000001D,
    PJ_SSL_FORTEZZA_KEA_WITH_RC4_128_SHA     	= 0x0000001E,
    
    /* SSLv2 */
    PJ_SSL_CK_RC4_128_WITH_MD5               	= 0x00010080,
    PJ_SSL_CK_RC4_128_EXPORT40_WITH_MD5      	= 0x00020080,
    PJ_SSL_CK_RC2_128_CBC_WITH_MD5           	= 0x00030080,
    PJ_SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5  	= 0x00040080,
    PJ_SSL_CK_IDEA_128_CBC_WITH_MD5          	= 0x00050080,
    PJ_SSL_CK_DES_64_CBC_WITH_MD5            	= 0x00060040,
    PJ_SSL_CK_DES_192_EDE3_CBC_WITH_MD5      	= 0x000700C0

} pj_ssl_cipher;


/**
 * Get cipher list supported by SSL/TLS backend.
 *
 * @param ciphers	The ciphers buffer to receive cipher list.
 * @param cipher_num	Maximum number of ciphers to be received.
 *
 * @return		PJ_SUCCESS when successful.
 */
PJ_DECL(pj_status_t) pj_ssl_cipher_get_availables(pj_ssl_cipher ciphers[],
					          unsigned *cipher_num);


/**
 * Check if the specified cipher is supported by SSL/TLS backend.
 *
 * @param cipher	The cipher.
 *
 * @return		PJ_TRUE when supported.
 */
PJ_DECL(pj_bool_t) pj_ssl_cipher_is_supported(pj_ssl_cipher cipher);


/**
 * Get cipher name string.
 *
 * @param cipher	The cipher.
 *
 * @return		The cipher name or NULL if cipher is not recognized/
 *			supported.
 */
PJ_DECL(const char*) pj_ssl_cipher_name(pj_ssl_cipher cipher);


/**
 * Get cipher ID from cipher name string. Note that on different backends
 * (e.g. OpenSSL or Symbian implementation), cipher names may not be
 * equivalent for the same cipher ID.
 *
 * @param cipher_name	The cipher name string.
 *
 * @return		The cipher ID or PJ_TLS_UNKNOWN_CIPHER if the cipher
 *			name string is not recognized/supported.
 */
PJ_DECL(pj_ssl_cipher) pj_ssl_cipher_id(const char *cipher_name);

/**
 * Elliptic curves enumeration.
 */
typedef enum pj_ssl_curve
{
	PJ_TLS_UNKNOWN_CURVE 		= 0,
	PJ_TLS_CURVE_SECT163K1		= 1,
	PJ_TLS_CURVE_SECT163R1		= 2,
	PJ_TLS_CURVE_SECT163R2		= 3,
	PJ_TLS_CURVE_SECT193R1		= 4,
	PJ_TLS_CURVE_SECT193R2		= 5,
	PJ_TLS_CURVE_SECT233K1		= 6,
	PJ_TLS_CURVE_SECT233R1		= 7,
	PJ_TLS_CURVE_SECT239K1		= 8,
	PJ_TLS_CURVE_SECT283K1		= 9,
	PJ_TLS_CURVE_SECT283R1		= 10,
	PJ_TLS_CURVE_SECT409K1		= 11,
	PJ_TLS_CURVE_SECT409R1		= 12,
	PJ_TLS_CURVE_SECT571K1		= 13,
	PJ_TLS_CURVE_SECT571R1		= 14,
	PJ_TLS_CURVE_SECP160K1		= 15,
	PJ_TLS_CURVE_SECP160R1		= 16,
	PJ_TLS_CURVE_SECP160R2		= 17,
	PJ_TLS_CURVE_SECP192K1		= 18,
	PJ_TLS_CURVE_SECP192R1		= 19,
	PJ_TLS_CURVE_SECP224K1		= 20,
	PJ_TLS_CURVE_SECP224R1		= 21,
	PJ_TLS_CURVE_SECP256K1		= 22,
	PJ_TLS_CURVE_SECP256R1		= 23,
	PJ_TLS_CURVE_SECP384R1		= 24,
	PJ_TLS_CURVE_SECP521R1		= 25,
	PJ_TLS_CURVE_BRAINPOOLP256R1	= 26,
	PJ_TLS_CURVE_BRAINPOOLP384R1	= 27,
	PJ_TLS_CURVE_BRAINPOOLP512R1	= 28,
	PJ_TLS_CURVE_ARBITRARY_EXPLICIT_PRIME_CURVES	= 0XFF01,
	PJ_TLS_CURVE_ARBITRARY_EXPLICIT_CHAR2_CURVES	= 0XFF02
} pj_ssl_curve;

/**
 * Get curve list supported by SSL/TLS backend.
 *
 * @param curves	The curves buffer to receive curve list.
 * @param curve_num	Maximum number of curves to be received.
 *
 * @return		PJ_SUCCESS when successful.
 */
PJ_DECL(pj_status_t) pj_ssl_curve_get_availables(pj_ssl_curve curves[],
					         unsigned *curve_num);

/**
 * Check if the specified curve is supported by SSL/TLS backend.
 *
 * @param curve		The curve.
 *
 * @return		PJ_TRUE when supported.
 */
PJ_DECL(pj_bool_t) pj_ssl_curve_is_supported(pj_ssl_curve curve);


/**
 * Get curve name string.
 *
 * @param curve		The curve.
 *
 * @return		The curve name or NULL if curve is not recognized/
 *			supported.
 */
PJ_DECL(const char*) pj_ssl_curve_name(pj_ssl_curve curve);

/**
 * Get curve ID from curve name string. Note that on different backends
 * (e.g. OpenSSL or Symbian implementation), curve names may not be
 * equivalent for the same curve ID.
 *
 * @param curve_name	The curve name string.
 *
 * @return		The curve ID or PJ_TLS_UNKNOWN_CURVE if the curve
 *			name string is not recognized/supported.
 */
PJ_DECL(pj_ssl_curve) pj_ssl_curve_id(const char *curve_name);

/*
 * Entropy enumeration
 */
typedef enum pj_ssl_entropy
{
	PJ_SSL_ENTROPY_NONE	= 0,
	PJ_SSL_ENTROPY_EGD	= 1,
	PJ_SSL_ENTROPY_RANDOM	= 2,
	PJ_SSL_ENTROPY_URANDOM	= 3,
	PJ_SSL_ENTROPY_FILE	= 4,
	PJ_SSL_ENTROPY_UNKNOWN	= 0x0F
} pj_ssl_entropy_t;

/**
 * This structure contains the callbacks to be called by the secure socket.
 */
typedef struct pj_ssl_sock_cb
{
    /**
     * This callback is called when a data arrives as the result of
     * pj_ssl_sock_start_read().
     *
     * @param ssock	The secure socket.
     * @param data	The buffer containing the new data, if any. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument may be NULL.
     * @param size	The length of data in the buffer.
     * @param status	The status of the read operation. This may contain
     *			non-PJ_SUCCESS for example when the TCP connection
     *			has been closed. In this case, the buffer may
     *			contain left over data from previous callback which
     *			the application may want to process.
     * @param remainder	If application wishes to leave some data in the 
     *			buffer (common for TCP applications), it should 
     *			move the remainder data to the front part of the 
     *			buffer and set the remainder length here. The value
     *			of this parameter will be ignored for datagram
     *			sockets.
     *
     * @return		PJ_TRUE if further read is desired, and PJ_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the secure socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_read)(pj_ssl_sock_t *ssock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder);
    /**
     * This callback is called when a packet arrives as the result of
     * pj_ssl_sock_start_recvfrom().
     *
     * @param ssock	The secure socket.
     * @param data	The buffer containing the packet, if any. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument will be set to NULL.
     * @param size	The length of packet in the buffer. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument will be set to zero.
     * @param src_addr	Source address of the packet.
     * @param addr_len	Length of the source address.
     * @param status	This contains
     *
     * @return		PJ_TRUE if further read is desired, and PJ_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the secure socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_recvfrom)(pj_ssl_sock_t *ssock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status);

    /**
     * This callback is called when data has been sent.
     *
     * @param ssock	The secure socket.
     * @param send_key	Key associated with the send operation.
     * @param sent	If value is positive non-zero it indicates the
     *			number of data sent. When the value is negative,
     *			it contains the error code which can be retrieved
     *			by negating the value (i.e. status=-sent).
     *
     * @return		Application may destroy the secure socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_sent)(pj_ssl_sock_t *ssock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent);

    /**
     * This callback is called when new connection arrives as the result
     * of pj_ssl_sock_start_accept(). If the status of accept operation is
     * needed use on_accept_complete2 instead of this callback.
     *
     * @param ssock	The secure socket.
     * @param newsock	The new incoming secure socket.
     * @param src_addr	The source address of the connection.
     * @param addr_len	Length of the source address.
     *
     * @return		PJ_TRUE if further accept() is desired, and PJ_FALSE
     *			when application no longer wants to accept incoming
     *			connection. Application may destroy the secure socket
     *			in the callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_accept_complete)(pj_ssl_sock_t *ssock,
				    pj_ssl_sock_t *newsock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len);
    /**
     * This callback is called when new connection arrives as the result
     * of pj_ssl_sock_start_accept().
     *
     * @param asock	The active socket.
     * @param newsock	The new incoming socket.
     * @param src_addr	The source address of the connection.
     * @param addr_len	Length of the source address.
     * @param status	The status of the accept operation. This may contain
     *			non-PJ_SUCCESS for example when the TCP listener is in
     *			bad state for example on iOS platform after the
     *			application waking up from background.
     *
     * @return		PJ_TRUE if further accept() is desired, and PJ_FALSE
     *			when application no longer wants to accept incoming
     *			connection. Application may destroy the active socket
     *			in the callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_accept_complete2)(pj_ssl_sock_t *ssock,
				     pj_ssl_sock_t *newsock,
				     const pj_sockaddr_t *src_addr,
				     int src_addr_len, 
				     pj_status_t status);

    /**
     * This callback is called when pending connect operation has been
     * completed.
     *
     * @param ssock	The secure socket.
     * @param status	The connection result. If connection has been
     *			successfully established, the status will contain
     *			PJ_SUCCESS.
     *
     * @return		Application may destroy the secure socket in the
     *			callback and return PJ_FALSE here. 
     */
    pj_bool_t (*on_connect_complete)(pj_ssl_sock_t *ssock,
				     pj_status_t status);
    
    /**
     * This callback is called when certificate verification is being done.
     * Certification info can be obtained from #pj_ssl_sock_info. Currently
     * it's only implemented for OpenSSL backend.
     *
     * @param ssock	The secure socket.
     * @param is_server	PJ_TRUE to indicate an incoming connection.
     *
     * @return		Return PJ_TRUE if verification is successful. 
     *                  If verification failed, then the connection will be 
     *			dropped immediately.
     * 
     */
    pj_bool_t (*on_verify_cb)(pj_ssl_sock_t *ssock, pj_bool_t is_server);

} pj_ssl_sock_cb;


/** 
 * Enumeration of secure socket protocol types.
 * This can be combined using bitwise OR operation.
 */
typedef enum pj_ssl_sock_proto
{
    /**
     * Default protocol of backend. 
     */   
    PJ_SSL_SOCK_PROTO_DEFAULT = 0,

    /** 
     * SSLv2.0 protocol.	  
     */
    PJ_SSL_SOCK_PROTO_SSL2    = (1 << 0),

    /** 
     * SSLv3.0 protocol.	  
     */
    PJ_SSL_SOCK_PROTO_SSL3    = (1 << 1),

    /**
     * TLSv1.0 protocol.	  
     */
    PJ_SSL_SOCK_PROTO_TLS1    = (1 << 2),

    /** 
     * TLSv1.1 protocol.
     */
    PJ_SSL_SOCK_PROTO_TLS1_1  = (1 << 3),

    /**
     * TLSv1.2 protocol.
     */
    PJ_SSL_SOCK_PROTO_TLS1_2  = (1 << 4),

    /**
     * TLSv1.3 protocol.
     */
    PJ_SSL_SOCK_PROTO_TLS1_3  = (1 << 5),

    /** 
     * Certain backend implementation e.g:OpenSSL, has feature to enable all
     * protocol. 
     */
    PJ_SSL_SOCK_PROTO_SSL23   = (1 << 16) - 1,
    PJ_SSL_SOCK_PROTO_ALL = PJ_SSL_SOCK_PROTO_SSL23,

    /**
     * DTLSv1.0 protocol.	  
     */
    PJ_SSL_SOCK_PROTO_DTLS1   = (1 << 16),

} pj_ssl_sock_proto;


/**
 * Definition of secure socket info structure.
 */
typedef struct pj_ssl_sock_info 
{
    /**
     * Describes whether secure socket connection is established, i.e: TLS/SSL 
     * handshaking has been done successfully.
     */
    pj_bool_t established;

    /**
     * Describes secure socket protocol being used, see #pj_ssl_sock_proto. 
     * Use bitwise OR operation to combine the protocol type.
     */
    pj_uint32_t proto;

    /**
     * Describes cipher suite being used, this will only be set when connection
     * is established.
     */
    pj_ssl_cipher cipher;

    /**
     * Describes local address.
     */
    pj_sockaddr local_addr;

    /**
     * Describes remote address.
     */
    pj_sockaddr remote_addr;
   
    /**
     * Describes active local certificate info.
     */
    pj_ssl_cert_info *local_cert_info;
   
    /**
     * Describes active remote certificate info.
     */
    pj_ssl_cert_info *remote_cert_info;

    /**
     * Status of peer certificate verification.
     */
    pj_uint32_t		verify_status;

    /**
     * Last native error returned by the backend.
     */
    unsigned long	last_native_err;

    /**
     * Group lock assigned to the ioqueue key.
     */
    pj_grp_lock_t *grp_lock;

} pj_ssl_sock_info;


/**
 * Definition of secure socket creation parameters.
 */
typedef struct pj_ssl_sock_param
{
    /**
     * Optional group lock to be assigned to the ioqueue key.
     *
     * Note that when a secure socket listener is configured with a group
     * lock, any new secure socket of an accepted incoming connection
     * will have its own group lock created automatically by the library,
     * this group lock can be queried via pj_ssl_sock_get_info() in the info
     * field pj_ssl_sock_info::grp_lock.
     */
    pj_grp_lock_t *grp_lock;

    /**
     * Specifies socket address family, either pj_AF_INET() and pj_AF_INET6().
     *
     * Default is pj_AF_INET().
     */
    int sock_af;

    /**
     * Specify socket type, either pj_SOCK_DGRAM() or pj_SOCK_STREAM().
     *
     * Default is pj_SOCK_STREAM().
     */
    int sock_type;

    /**
     * Specify the ioqueue to use. Secure socket uses the ioqueue to perform
     * active socket operations, see \ref PJ_ACTIVESOCK for more detail.
     */
    pj_ioqueue_t *ioqueue;

    /**
     * Specify the timer heap to use. Secure socket uses the timer to provide
     * auto cancelation on asynchronous operation when it takes longer time 
     * than specified timeout period, e.g: security negotiation timeout.
     */
    pj_timer_heap_t *timer_heap;

    /**
     * Specify secure socket callbacks, see #pj_ssl_sock_cb.
     */
    pj_ssl_sock_cb cb;

    /**
     * Specify secure socket user data.
     */
    void *user_data;

    /**
     * Specify security protocol to use, see #pj_ssl_sock_proto. Use bitwise OR 
     * operation to combine the protocol type.
     *
     * Default is PJ_SSL_SOCK_PROTO_DEFAULT.
     */
    pj_uint32_t proto;

    /**
     * Number of concurrent asynchronous operations that is to be supported
     * by the secure socket. This value only affects socket receive and
     * accept operations -- the secure socket will issue one or more 
     * asynchronous read and accept operations based on the value of this
     * field. Setting this field to more than one will allow more than one
     * incoming data or incoming connections to be processed simultaneously
     * on multiprocessor systems, when the ioqueue is polled by more than
     * one threads.
     *
     * The default value is 1.
     */
    unsigned async_cnt;

    /**
     * The ioqueue concurrency to be forced on the socket when it is 
     * registered to the ioqueue. See #pj_ioqueue_set_concurrency() for more
     * info about ioqueue concurrency.
     *
     * When this value is -1, the concurrency setting will not be forced for
     * this socket, and the socket will inherit the concurrency setting of 
     * the ioqueue. When this value is zero, the secure socket will disable
     * concurrency for the socket. When this value is +1, the secure socket
     * will enable concurrency for the socket.
     *
     * The default value is -1.
     */
    int concurrency;

    /**
     * If this option is specified, the secure socket will make sure that
     * asynchronous send operation with stream oriented socket will only
     * call the callback after all data has been sent. This means that the
     * secure socket will automatically resend the remaining data until
     * all data has been sent.
     *
     * Please note that when this option is specified, it is possible that
     * error is reported after partial data has been sent. Also setting
     * this will disable the ioqueue concurrency for the socket.
     *
     * Default value is 1.
     */
    pj_bool_t whole_data;

    /**
     * Specify buffer size for sending operation. Buffering sending data
     * is used for allowing application to perform multiple outstanding 
     * send operations. Whenever application specifies this setting too
     * small, sending operation may return PJ_ENOMEM.
     *  
     * Default value is 8192 bytes.
     */
    pj_size_t send_buffer_size;

    /**
     * Specify buffer size for receiving encrypted (and perhaps compressed)
     * data on underlying socket. This setting is unused on Symbian, since 
     * SSL/TLS Symbian backend, CSecureSocket, can use application buffer 
     * directly.
     *
     * Default value is 1500.
     */
    pj_size_t read_buffer_size;

    /**
     * Number of ciphers contained in the specified cipher preference. 
     * If this is set to zero, then the cipher list used will be determined
     * by the backend default (for OpenSSL backend, setting 
     * PJ_SSL_SOCK_OSSL_CIPHERS will be used).
     */
    unsigned ciphers_num;

    /**
     * Ciphers and order preference. If empty, then default cipher list and
     * its default order of the backend will be used.
     */
    pj_ssl_cipher *ciphers;

    /**
     * Number of curves contained in the specified curve preference.
     * If this is set to zero, then default curve list of the backend
     * will be used.
     *
     * Default: 0 (zero).
     */
    unsigned curves_num;

    /**
     * Curves and order preference. The #pj_ssl_curve_get_availables()
     * can be used to check the available curves supported by backend.
     */
    pj_ssl_curve *curves;

    /**
     * The supported signature algorithms. Set the sigalgs string
     * using this form:
     * "<DIGEST>+<ALGORITHM>:<DIGEST>+<ALGORITHM>"
     * Digests are: "RSA", "DSA" or "ECDSA"
     * Algorithms are: "MD5", "SHA1", "SHA224", "SHA256", "SHA384", "SHA512"
     * Example: "ECDSA+SHA256:RSA+SHA256"
     */
    pj_str_t	sigalgs;

    /**
     * Reseed random number generator.
     * For type #PJ_SSL_ENTROPY_FILE, parameter \a entropy_path
     * must be set to a file.
     * For type #PJ_SSL_ENTROPY_EGD, parameter \a entropy_path
     * must be set to a socket.
     *
     * Default value is PJ_SSL_ENTROPY_NONE.
    */
    pj_ssl_entropy_t	entropy_type;

    /**
     * When using a file/socket for entropy #PJ_SSL_ENTROPY_EGD or
     * #PJ_SSL_ENTROPY_FILE, \a entropy_path must contain the path
     * to entropy socket/file.
     *
     * Default value is an empty string.
     */
    pj_str_t		entropy_path;

    /**
     * Security negotiation timeout. If this is set to zero (both sec and 
     * msec), the negotiation doesn't have a timeout.
     *
     * Default value is zero.
     */
    pj_time_val	timeout;

    /**
     * Specify whether endpoint should verify peer certificate.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t verify_peer;
    
    /**
     * When secure socket is acting as server (handles incoming connection),
     * it will require the client to provide certificate.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t require_client_cert;

    /**
     * Server name indication. When secure socket is acting as client 
     * (perform outgoing connection) and the server may host multiple
     * 'virtual' servers at a single underlying network address, setting
     * this will allow client to tell the server a name of the server
     * it is contacting. This must be set to hostname and literal IP addresses
     * are not allowed.
     *
     * Default value is zero/not-set.
     */
    pj_str_t server_name;

    /**
     * Specify if SO_REUSEADDR should be used for listening socket. This
     * option will only be used with accept() operation.
     *
     * Default is PJ_FALSE.
     */
    pj_bool_t reuse_addr;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qos_param fields since this is more portable.
     *
     * Default value is PJ_QOS_TYPE_BEST_EFFORT.
     */
    pj_qos_type qos_type;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qos_type field and may not be
     * supported on all platforms.
     *
     * By default all settings in this structure are disabled.
     */
    pj_qos_params qos_params;

    /**
     * Specify if the transport should ignore any errors when setting the QoS
     * traffic type/parameters.
     *
     * Default: PJ_TRUE
     */
    pj_bool_t qos_ignore_error;

    /**
     * Specify options to be set on the transport. 
     *
     * By default there is no options.
     * 
     */
    pj_sockopt_params sockopt_params;

    /**
     * Specify if the transport should ignore any errors when setting the 
     * sockopt parameters.
     *
     * Default: PJ_TRUE
     * 
     */
    pj_bool_t sockopt_ignore_error;

} pj_ssl_sock_param;


/**
 * The parameter for pj_ssl_sock_start_connect2().
 */
typedef struct pj_ssl_start_connect_param {
    /**
     * The pool to allocate some internal data for the operation.
     */
    pj_pool_t *pool;

    /**
     * Local address.
     */
    const pj_sockaddr_t *localaddr;

    /**
     * Port range for socket binding, relative to the start port number
     * specified in \a localaddr. This is only applicable when the start port
     * number is non zero.
     */
    pj_uint16_t local_port_range;

    /**
     * Remote address.
     */
    const pj_sockaddr_t *remaddr;

    /**
     * Length of buffer containing above addresses.
     */
    int addr_len;

} pj_ssl_start_connect_param;


/**
 * Initialize the secure socket parameters for its creation with 
 * the default values.
 *
 * @param param		The parameter to be initialized.
 */
PJ_DECL(void) pj_ssl_sock_param_default(pj_ssl_sock_param *param);


/**
 * Duplicate pj_ssl_sock_param.
 *
 * @param pool	Pool to allocate memory.
 * @param dst	Destination parameter.
 * @param src	Source parameter.
 */
PJ_DECL(void) pj_ssl_sock_param_copy(pj_pool_t *pool, 
				     pj_ssl_sock_param *dst,
				     const pj_ssl_sock_param *src);


/**
 * Create secure socket instance.
 *
 * @param pool		The pool for allocating secure socket instance.
 * @param param		The secure socket parameter, see #pj_ssl_sock_param.
 * @param p_ssock	Pointer to secure socket instance to be created.
 *
 * @return		PJ_SUCCESS when successful.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_create(pj_pool_t *pool,
					const pj_ssl_sock_param *param,
					pj_ssl_sock_t **p_ssock);


/**
 * Set secure socket certificate or credentials. Credentials may include 
 * certificate, private key and trusted Certification Authorities list. 
 * Normally, server socket must provide certificate (and private key).
 * Socket client may also need to provide certificate in case requested
 * by the server.
 *
 * @param ssock		The secure socket instance.
 * @param pool		The pool.
 * @param cert		The endpoint certificate/credentials, see
 *			#pj_ssl_cert_t.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_set_certificate(
					    pj_ssl_sock_t *ssock,
					    pj_pool_t *pool,
					    const pj_ssl_cert_t *cert);


/**
 * Close and destroy the secure socket.
 *
 * @param ssock		The secure socket.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_close(pj_ssl_sock_t *ssock);


/**
 * Associate arbitrary data with the secure socket. Application may
 * inspect this data in the callbacks and associate it with higher
 * level processing.
 *
 * @param ssock		The secure socket.
 * @param user_data	The user data to be associated with the secure
 *			socket.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_set_user_data(pj_ssl_sock_t *ssock,
					       void *user_data);

/**
 * Retrieve the user data previously associated with this secure
 * socket.
 *
 * @param ssock		The secure socket.
 *
 * @return		The user data.
 */
PJ_DECL(void*) pj_ssl_sock_get_user_data(pj_ssl_sock_t *ssock);


/**
 * Retrieve the local address and port used by specified secure socket.
 *
 * @param ssock		The secure socket.
 * @param info		The info buffer to be set, see #pj_ssl_sock_info.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_get_info(pj_ssl_sock_t *ssock,
					  pj_ssl_sock_info *info);


/**
 * Starts read operation on this secure socket. This function will create
 * \a async_cnt number of buffers (the \a async_cnt parameter was given
 * in \a pj_ssl_sock_create() function) where each buffer is \a buff_size
 * long. The buffers are allocated from the specified \a pool. Once the 
 * buffers are created, it then issues \a async_cnt number of asynchronous
 * \a recv() operations to the socket and returns back to caller. Incoming
 * data on the socket will be reported back to application via the 
 * \a on_data_read() callback.
 *
 * Application only needs to call this function once to initiate read
 * operations. Further read operations will be done automatically by the
 * secure socket when \a on_data_read() callback returns non-zero. 
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param flags		Flags to be given to pj_ioqueue_recv().
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_read(pj_ssl_sock_t *ssock,
					    pj_pool_t *pool,
					    unsigned buff_size,
					    pj_uint32_t flags);

/**
 * Same as #pj_ssl_sock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param readbuf	Array of packet buffers, each has buff_size size.
 * @param flags		Flags to be given to pj_ioqueue_recv().
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_read2(pj_ssl_sock_t *ssock,
					     pj_pool_t *pool,
					     unsigned buff_size,
					     void *readbuf[],
					     pj_uint32_t flags);

/**
 * Same as pj_ssl_sock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param flags		Flags to be given to pj_ioqueue_recvfrom().
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_recvfrom(pj_ssl_sock_t *ssock,
						pj_pool_t *pool,
						unsigned buff_size,
						pj_uint32_t flags);

/**
 * Same as #pj_ssl_sock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param readbuf	Array of packet buffers, each has buff_size size.
 * @param flags		Flags to be given to pj_ioqueue_recvfrom().
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_recvfrom2(pj_ssl_sock_t *ssock,
						 pj_pool_t *pool,
						 unsigned buff_size,
						 void *readbuf[],
						 pj_uint32_t flags);

/**
 * Send data using the socket.
 *
 * @param ssock		The secure socket.
 * @param send_key	The operation key to send the data, which is useful
 *			if application wants to submit multiple pending
 *			send operations and want to track which exact data 
 *			has been sent in the \a on_data_sent() callback.
 * @param data		The data to be sent. This data must remain valid
 *			until the data has been sent.
 * @param size		The size of the data.
 * @param flags		Flags to be given to pj_ioqueue_send().
 *
 * @return		PJ_SUCCESS if data has been sent immediately, or
 *			PJ_EPENDING if data cannot be sent immediately or
 *			PJ_ENOMEM when sending buffer could not handle all
 *			queued data, see \a send_buffer_size. The callback
 *			\a on_data_sent() will be called when data is actually
 *			sent. Any other return value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_send(pj_ssl_sock_t *ssock,
				      pj_ioqueue_op_key_t *send_key,
				      const void *data,
				      pj_ssize_t *size,
				      unsigned flags);

/**
 * Send datagram using the socket.
 *
 * @param ssock		The secure socket.
 * @param send_key	The operation key to send the data, which is useful
 *			if application wants to submit multiple pending
 *			send operations and want to track which exact data 
 *			has been sent in the \a on_data_sent() callback.
 * @param data		The data to be sent. This data must remain valid
 *			until the data has been sent.
 * @param size		The size of the data.
 * @param flags		Flags to be given to pj_ioqueue_send().
 * @param addr		The destination address.
 * @param addr_len	Length of buffer containing destination address.
 *
 * @return		PJ_SUCCESS if data has been sent immediately, or
 *			PJ_EPENDING if data cannot be sent immediately. In
 *			this case the \a on_data_sent() callback will be
 *			called when data is actually sent. Any other return
 *			value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_sendto(pj_ssl_sock_t *ssock,
					pj_ioqueue_op_key_t *send_key,
					const void *data,
					pj_ssize_t *size,
					unsigned flags,
					const pj_sockaddr_t *addr,
					int addr_len);


/**
 * Starts asynchronous socket accept() operations on this secure socket. 
 * This function will issue \a async_cnt number of asynchronous \a accept() 
 * operations to the socket and returns back to caller. Incoming
 * connection on the socket will be reported back to application via the
 * \a on_accept_complete() callback.
 *
 * Application only needs to call this function once to initiate accept()
 * operations. Further accept() operations will be done automatically by 
 * the secure socket when \a on_accept_complete() callback returns non-zero.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate some internal data for the
 *			operation.
 * @param local_addr	Local address to bind on.
 * @param addr_len	Length of buffer containing local address.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_accept(pj_ssl_sock_t *ssock,
					      pj_pool_t *pool,
					      const pj_sockaddr_t *local_addr,
					      int addr_len);


/**
 * Same as #pj_ssl_sock_start_accept(), but application can provide
 * a secure socket parameter, which will be used to create a new secure
 * socket reported in \a on_accept_complete() callback when there is
 * an incoming connection.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate some internal data for the
 *			operation.
 * @param local_addr	Local address to bind on.
 * @param addr_len	Length of buffer containing local address.
 * @param newsock_param	Secure socket parameter for new accepted sockets.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t)
pj_ssl_sock_start_accept2(pj_ssl_sock_t *ssock,
			  pj_pool_t *pool,
			  const pj_sockaddr_t *local_addr,
			  int addr_len,
			  const pj_ssl_sock_param *newsock_param);


/**
 * Starts asynchronous socket connect() operation and SSL/TLS handshaking 
 * for this socket. Once the connection is done (either successfully or not),
 * the \a on_connect_complete() callback will be called.
 *
 * @param ssock		The secure socket.
 * @param pool		The pool to allocate some internal data for the
 *			operation.
 * @param localaddr	Local address.
 * @param remaddr	Remote address.
 * @param addr_len	Length of buffer containing above addresses.
 *
 * @return		PJ_SUCCESS if connection can be established immediately
 *			or PJ_EPENDING if connection cannot be established 
 *			immediately. In this case the \a on_connect_complete()
 *			callback will be called when connection is complete. 
 *			Any other return value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_connect(pj_ssl_sock_t *ssock,
					       pj_pool_t *pool,
					       const pj_sockaddr_t *localaddr,
					       const pj_sockaddr_t *remaddr,
					       int addr_len);

/**
 * Same as #pj_ssl_sock_start_connect(), but application can provide a 
 * \a port_range parameter, which will be used to bind the socket to 
 * random port.
 *
 * @param ssock		The secure socket.
 *
 * @param connect_param The parameter, refer to \a pj_ssl_start_connect_param.
 *
 * @return		PJ_SUCCESS if connection can be established immediately
 *			or PJ_EPENDING if connection cannot be established 
 *			immediately. In this case the \a on_connect_complete()
 *			callback will be called when connection is complete. 
 *			Any other return value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_connect2(
			      pj_ssl_sock_t *ssock,
			      pj_ssl_start_connect_param *connect_param);

/**
 * Starts SSL/TLS renegotiation over an already established SSL connection
 * for this socket. This operation is performed transparently, no callback 
 * will be called once the renegotiation completed successfully. However, 
 * when the renegotiation fails, the connection will be closed and callback
 * \a on_data_read() will be invoked with non-PJ_SUCCESS status code.
 *
 * @param ssock		The secure socket.
 *
 * @return		PJ_SUCCESS if renegotiation is completed immediately,
 *			or PJ_EPENDING if renegotiation has been started and
 *			waiting for completion, or the appropriate error code 
 *			on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_renegotiate(pj_ssl_sock_t *ssock);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJ_SSL_SOCK_H__ */
