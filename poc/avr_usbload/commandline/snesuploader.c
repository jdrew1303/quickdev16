/* Name: set-led.c
 * Project: custom-class, a basic USB example
 * Author: Christian Starkjohann
 * Creation Date: 2008-04-10
 * Tabsize: 4
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 * This Revision: $Id: set-led.c 692 2008-11-07 15:07:40Z cs $
 */

/*
General Description:
This is the host-side driver for the custom-class example device. It searches
the USB for the LEDControl device and sends the requests understood by this
device.
This program must be linked with libusb on Unix and libusb-win32 on Windows.
See http://libusb.sourceforge.net/ or http://libusb-win32.sourceforge.net/
respectively.
*/


#define BUFFER_SIZE     8
#define BUFFER_CRC      (1024 * 64)
#define BANK_SIZE       (1<<15)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>        /* this is libusb */
#include "opendevice.h" /* common code moved to separate module */

#include "../requests.h"   /* custom request numbers */
#include "../usbconfig.h"  /* device's VID/PID and names */

uint16_t crc_xmodem_update (uint16_t crc, uint8_t data){
    int i;
    crc = crc ^ ((uint16_t)data << 8);
    for (i=0; i<8; i++)
    {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }

    return crc;
}

uint16_t do_crc(uint8_t * data,uint16_t size){
	uint16_t crc =0;
	uint16_t i;
	for (i=0; i<size; i++){
		crc = crc_xmodem_update(crc,data[i]);
	}
  	return crc;
}


uint16_t do_crc_update(uint16_t crc,uint8_t * data,uint16_t size){
  uint16_t i;
  for (i=0; i<size; i++)
	crc = crc_xmodem_update(crc,data[i]);
  return crc;
}



static void usage(char *name)
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s upload filename.. upload\n", name);
}


int main(int argc, char **argv)
{
    usb_dev_handle      *handle = NULL;
    const unsigned char rawVid[2] = {USB_CFG_VENDOR_ID}, rawPid[2] = {USB_CFG_DEVICE_ID};
    char                vendor[] = {USB_CFG_VENDOR_NAME, 0}, product[] = {USB_CFG_DEVICE_NAME, 0};
    int                 cnt, vid, pid;
    int                 cnt_crc = 0;
    unsigned char       *read_buffer;       
    unsigned char       *crc_buffer;       
    unsigned char       setup_buffer[8];       
    unsigned int        addr = 0;
    unsigned int        bank_addr;
    unsigned int        bank_num;
    uint16_t            crc = 0;
    FILE                *fp ;

    usb_init();
    if(argc < 2){   /* we need at least one argument */
        usage(argv[0]);
        exit(1);
    }
    /* compute VID/PID from usbconfig.h so that there is a central source of information */
    vid = rawVid[1] * 256 + rawVid[0];
    pid = rawPid[1] * 256 + rawPid[0];
    /* The following function is in opendevice.c: */
    if(usbOpenDevice(&handle, vid, vendor, pid, product, NULL, NULL, NULL) != 0){
        fprintf(stderr, "Could not find USB device \"%s\" with vid=0x%x pid=0x%x\n", product, vid, pid);
        exit(1);
    }

    if(strcasecmp(argv[1], "upload") == 0){
        if(argc < 3){   /* we need at least one argument */
            usage(argv[0]);
            exit(1);
        }
        fp = fopen (argv[2],"r") ;
        if (fp==NULL){
            fprintf(stderr, "Cannot open file %s ", argv[2]);
            exit(1);
        }
        read_buffer = (unsigned char*)malloc(BUFFER_SIZE);
        crc_buffer = (unsigned char*)malloc(BUFFER_CRC);
        memset(crc_buffer,0,BUFFER_CRC);
        addr = 0x000000;
        bank_addr = addr& 0xffff;
        bank_num =  (addr>>16) &  0xff;
  
        while((cnt = fread(read_buffer, BUFFER_SIZE, 1, fp)) > 0){
            bank_addr = addr& 0xffff;
            bank_num =  (addr>>16) &  0xff;
            
            memcpy(crc_buffer + cnt_crc,read_buffer,BUFFER_SIZE);
            cnt_crc += BUFFER_SIZE;
            if (cnt_crc == BANK_SIZE){
                crc = do_crc(crc_buffer,BANK_SIZE);
                printf("Addr: 0x%06x Bank: 0x%02x Rom Addr: 0x%04x  Addr: 0x%06x Cnt: 0x%04x\n",addr,bank_num, bank_addr, addr,crc);
                memset(crc_buffer,0,BUFFER_CRC);
                cnt_crc =0;
            }
            usb_control_msg(handle, 
          			   USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, 
          			   CUSTOM_RQ_UPLOAD, bank_num, bank_addr, 
          			   (char*) read_buffer, BUFFER_SIZE, 
          			   5000);
            addr += BUFFER_SIZE;
            //printf("Addr: 0x%06x Bank: 0x%02x Rom Addr: 0x%04x  Addr: 0x%06x Cnt: 0x%04x cnt: %i\n",addr,bank_num, bank_addr, addr,crc,cnt_crc);
        }
        cnt = usb_control_msg(handle, 
      			   USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, 
      			   CUSTOM_RQ_CRC_CHECK, bank_num + 1, 0, 
      			   (char*) setup_buffer, sizeof(setup_buffer), 
      			   5000);
        
        if(cnt < 1){
            if(cnt < 0){
                fprintf(stderr, "USB error: %s\n", usb_strerror());
            }else{
                fprintf(stderr, "only %d bytes received.\n", cnt);
            }
        }
    }else{
        usage(argv[0]);
        exit(1);
    }
    usb_close(handle);
    return 0;
}
