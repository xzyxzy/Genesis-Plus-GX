#ifndef __IPS_H__
#define __IPS_H__

int ips_patch(
    void *buf_rom, int len_rom,
    void *buf_ips, int len_ips
);

#endif