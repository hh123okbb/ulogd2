/* ulogd_PWSNIFF.c, Version $Revision: 1.3 $
 *
 * ulogd logging interpreter for POP3 / FTP like plaintext passwords.
 *
 * (C) 2000 by Harald Welte <laforge@gnumonks.org>
 * This software is released under the terms of GNU GPL
 *
 * $Id: ulogd_PWSNIFF.c,v 1.3 2000/11/16 17:20:52 laforge Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ulogd.h>
#include <string.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>

#ifdef DEBUG_PWSNIFF
#define DEBUGP(x) ulogd_log(ULOGD_DEBUG, x)
#else
#define DEBUGP(format, args...)
#endif


#define PORT_POP3	110
#define PORT_FTP	21

static u_int16_t pwsniff_ports[] = {
	__constant_htons(PORT_POP3),
	__constant_htons(PORT_FTP),
};

#define PWSNIFF_MAX_PORTS 2

static char *_get_next_blank(char* begp, char *endp)
{
	char *ptr;

	for (ptr = begp; ptr < endp; ptr++) {
		if (*ptr == ' ' || *ptr == '\n' || *ptr == '\r') {
			return ptr-1;	
		}
	}
	return NULL;
}

static ulog_iret_t *_interp_pwsniff(ulog_interpreter_t *ip, ulog_packet_msg_t *pkt)
{
	struct iphdr *iph = (struct iphdr *) pkt->payload;
	void *protoh = (u_int32_t *)iph + iph->ihl;
	struct tcphdr *tcph = protoh;
	u_int32_t tcplen = ntohs(iph->tot_len) - iph->ihl * 4;
	unsigned char  *ptr, *begp, *pw_begp, *endp, *pw_endp;
	ulog_iret_t *ret = ip->result;
	int len, pw_len, i, cont = 0;

	len = pw_len = 0;
	begp = pw_begp = NULL;

	if (iph->protocol != IPPROTO_TCP)
		return NULL;
	
	for (i = 0; i <= PWSNIFF_MAX_PORTS; i++)
	{
		if (tcph->dest == pwsniff_ports[i]) {
			cont = 1; 
			break;
		}
	}
	if (!cont)
		return NULL;

	DEBUGP("----> pwsniff detected, tcplen=%d, struct=%d, iphtotlen=%d, ihl=%d\n", tcplen, sizeof(struct tcphdr), ntohs(iph->tot_len), iph->ihl);

	for (ptr = (unsigned char *) tcph + sizeof(struct tcphdr); 
			ptr < (unsigned char *) tcph + tcplen; ptr++)
	{
		if (!strncasecmp(ptr, "USER ", 5)) {
			begp = ptr+5;
			endp = _get_next_blank(begp, (char *)tcph + tcplen);
			if (endp)
				len = endp - begp + 1;
		}
		if (!strncasecmp(ptr, "PASS ", 5)) {
			pw_begp = ptr+5;
			pw_endp = _get_next_blank(pw_begp, 
					(char *)tcph + tcplen);
			if (pw_endp)
				pw_len = pw_endp - pw_begp + 1;
		}
	}

	if (len) {
		ret[0].value.ptr = (char *) malloc(len+1);
		ret[0].flags |= ULOGD_RETF_VALID;
		if (!ret[0].value.ptr) {
			ulogd_log(ULOGD_ERROR, "OOM (size=%u)\n", len);
			return NULL;
		}
		strncpy(ret[0].value.ptr, begp, len);
		*((char *)ret[0].value.ptr + len + 1) = '\0';
	}
	if (pw_len) {
		ret[1].value.ptr = (char *) malloc(pw_len+1);
		ret[1].flags |= ULOGD_RETF_VALID;
		if (!ret[1].value.ptr){
			ulogd_log(ULOGD_ERROR, "OOM (size=%u)\n", pw_len);
			return NULL;
		}
		strncpy(ret[1].value.ptr, pw_begp, pw_len);
		*((char *)ret[1].value.ptr + pw_len + 1) = '\0';

	}
	return ret;
}

static ulog_iret_t pwsniff_rets[] = {
	{ NULL, NULL, 0, ULOGD_RET_STRING, ULOGD_RETF_FREE, "pwsniff.user", 
		{ ptr: NULL } },
	{ NULL, NULL, 0, ULOGD_RET_STRING, ULOGD_RETF_FREE, "pwsniff.pass", 
		{ ptr: NULL } },
};
static ulog_interpreter_t base_ip[] = { 

	{ NULL, "pwsniff", 0, &_interp_pwsniff, 2, &pwsniff_rets },
	{ NULL, "", 0, NULL, 0, NULL }, 
};
void _base_reg_ip(void)
{
	ulog_interpreter_t *ip = base_ip;
	ulog_interpreter_t *p;

	for (p = ip; p->interp; p++)
		register_interpreter(p);

}


void _init(void)
{
	_base_reg_ip();
}