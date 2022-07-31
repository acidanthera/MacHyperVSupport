//
//  main.c
//  hvshutdown
//
//  Created by John Davis on 7/27/22.
//

#include <stdio.h>
#include <IOKit/IOKitLib.h>

#include <mach/mach_port.h>

#include "HyperVShutdownUserClient.h"

io_connect_t hvshutdown_connect(void) {
  io_connect_t con = 0;
  CFMutableDictionaryRef dict = IOServiceMatching("HyperVShutdown");
  if (dict) {
    io_iterator_t iterator = 0;
    kern_return_t result = IOServiceGetMatchingServices(kIOMasterPortDefault, dict, &iterator);
    if (result == kIOReturnSuccess && IOIteratorIsValid(iterator)) {
      io_object_t device = IOIteratorNext(iterator);
      if (device) {
        result = IOServiceOpen(device, mach_task_self(), 0, &con);
        if (result != kIOReturnSuccess)
          fprintf(stderr, "Unable to open AppleRTC1 service %08X\n", result);
        IOObjectRelease(device);
      } else {
        fprintf(stderr, "Unable to locate AppleRTC device\n");
      }
      IOObjectRelease(iterator);
    } else {
      fprintf(stderr, "Unable to get AppleRTC2 service %08X\n", result);
    }
  } else {
    fprintf(stderr, "Unable to create AppleRTC matching dict\n");
  }
  
  return con;
}

static void
waitForNotification(mach_port_t port)
{
    kern_return_t   kr;
  
  HyperVShutdownNotificationMessage msg;

    // Now wait for a notification.
    //
    kr = mach_msg(&msg.header, MACH_RCV_MSG,
                  0, sizeof(msg), port, 0, MACH_PORT_NULL);

   // if (kr != KERN_SUCCESS)
        printf("Error: mach_msg %x\n", kr);
//  else
//      printf("\n\n[message id=%x]\n", msg.hdr.h.msgh_id);
}

int main(int argc, const char * argv[]) {
  kern_return_t  kernResult;
  io_service_t  service;
  io_iterator_t   iterator;
  
  io_connect_t con = hvshutdown_connect();
  
  mach_port_t p = MACH_PORT_NULL;
  mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &p);
  mach_port_insert_right(mach_task_self(), p, p, MACH_MSG_TYPE_MAKE_SEND);
  
  
  kern_return_t res = IOConnectSetNotificationPort (con, kHyperVShutdownNotificationTypePerformShutdown, p, 0);
  printf("Hello, World! %u %X\n", con, res);
  waitForNotification (p);
  
  
  pid_t p2;
      int status;
  
  p2 = fork();
      if (p2 == 0) {
        
  const char *shutdownArgs[] = {
    "/sbin/shutdown",
    "-h",
    "now",
    NULL
  };
  int ret = execv ("/sbin/shutdown", shutdownArgs);
  if(ret == -1) {
    printf("error %u\n", ret);
          }
      }
  
  do {
    p2 = wait(&status);
      } while (p2 == -1 && errno == EINTR);
  
  //while (true);
  
  // insert code here...
  
  return 0;
}
