/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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

/* TF-PSA-Crypto user configuration, named by
 * CONFIG_TF_PSA_CRYPTO_USER_CONFIG_FILE in overlay-tls.conf. With threads,
 * pjlib's TLS sockets and pj_ssl_rand_bytes() call PSA Crypto from more
 * than one thread, which needs Mbed TLS's threading layer over pthreads.
 * Zephyr has no Kconfig option for it.
 */
#if defined(CONFIG_PJPROJECT_THREADS)
#   define MBEDTLS_THREADING_C
#   define MBEDTLS_THREADING_PTHREAD
#endif
