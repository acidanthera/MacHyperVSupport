//
//  HyperVBalloonPrivate.cpp
//  Hyper-V balloon driver internal implementation
//
//  Copyright © 2022-2023 xdqi. All rights reserved.
//

#include <string.h>
#include <mach/vm_statistics.h>
#include <kern/host.h>
#include <sys/sysctl.h>

#include "HyperVBalloon.hpp"
#include "HyperVBalloonRegs.hpp"

bool HyperVBalloon::setupBalloon() {
  // Protocol version negotiation
  if (!doProtocolNegotitation(kHyperVDynamicMemoryProtocolVersion3, false) &&
      !doProtocolNegotitation(kHyperVDynamicMemoryProtocolVersion2, false) &&
      !doProtocolNegotitation(kHyperVDynamicMemoryProtocolVersion1, true)) {
    return false;
  }
  
  // Report capabilities to hypervisor
  HyperVDynamicMemoryMessage message, protoResponse;
  memset(&message, 0, sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageCapabilitiesReport));
  message.header.type = kDynamicMemoryMessageTypeCapabilitiesReport;
  message.header.size = sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageCapabilitiesReport);
  OSIncrementAtomic(&_transactionId);
  message.header.transactionId = _transactionId;
  message.capabilitiesReport.supportBalloon    = 1;  // We support balloon only
  message.capabilitiesReport.supportHotAdd     = 0;  // macOS doesn't support memory hot adding.
  message.capabilitiesReport.hotAddAlignment   = 0;
  message.capabilitiesReport.minimumPageCount  = 0;  // the same as
  message.capabilitiesReport.maximumPageNumber = -1; // both Windows and Linux drivers
    
  HVDBGLOG("Posting dynamic memory capatibilities report transactionID %u", _transactionId);
  if (_hvDevice->writeInbandPacketWithTransactionId(&message, message.header.size, _transactionId, true, &protoResponse, sizeof(protoResponse)) != kIOReturnSuccess) {
    return false;
  }

  HVDBGLOG("Got dynamic memory capatibilities response of %u", protoResponse.capabilitiesResponse.isAccepted);
  if (!protoResponse.capabilitiesResponse.isAccepted) {
    return false;
  }

  return true;
}

bool HyperVBalloon::doProtocolNegotitation(HyperVDynamicMemoryProtocolVersion version, bool isLastAttempt) {
  HyperVDynamicMemoryMessage message, protoResponse;
  memset(&message, 0, sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageProtocolRequest));
  message.header.type = kDynamicMemoryMessageTypeProtocolRequest;
  message.header.size = sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageProtocolRequest);
  OSIncrementAtomic(&_transactionId);
  message.header.transactionId = _transactionId;
  message.protocolRequest.version = version;
  message.protocolRequest.isLastAttempt = isLastAttempt;
    
  HVDBGLOG("Trying with dynamic memory protocol of %u.%u transactionID %u", version >> 16, version & 0xffff, _transactionId);
  if (_hvDevice->writeInbandPacketWithTransactionId(&message, message.header.size, _transactionId, true, &protoResponse, sizeof(protoResponse)) != kIOReturnSuccess) {
    return false;
  }

  HVDBGLOG("Got dynamic memory protocol response of %u", protoResponse.protocolResponse.isAccepted);
  if (!protoResponse.protocolResponse.isAccepted) {
    return false;
  }
  
  HVDBGLOG("Using dynamic memory protocol version of %u.%u", version >> 16, version & 0xffff);
  return true;
}

void HyperVBalloon::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVDynamicMemoryMessage *balloonMessage = (HyperVDynamicMemoryMessage*) pktData;
  
  switch (balloonMessage->header.type) {
    case kDynamicMemoryMessageTypeProtocolResponse:
    case kDynamicMemoryMessageTypeCapabilitiesResponse:
      // Pass them to HyperVBalloon::start()
      void   *responseBuffer;
      UInt32 responseLength;

      //
      // The protocol response packet always has a transaction ID of 0, wake using fixed transaction ID.
      // This assumes the response length is of the correct size, set before the initial request.
      //
      if (_hvDevice->getPendingTransaction(_transactionId, &responseBuffer, &responseLength)) {
        memcpy(responseBuffer, pktData, responseLength);
        _hvDevice->wakeTransaction(_transactionId);
      }
      break;

    case kDynamicMemoryMessageTypeBalloonInflationRequest:
      handleBalloonInflationRequest(&balloonMessage->inflationRequest);
      break;

    case kDynamicMemoryMessageTypeBalloonDeflationRequest:
      handleBalloonDeflationRequest(&balloonMessage->deflationRequest);
      break;

    case kDynamicMemoryMessageTypeHotAddRequest:
      handleHotAddRequest(&balloonMessage->hotAddRequest);
      break;

    case kDynamicMemoryMessageTypeInfoMessage:
      handleInformationMessage(&balloonMessage->information);
      break;
      
    default:
      HVDBGLOG("Unknown dynamic memory message type %u, size %u", balloonMessage->header.type, balloonMessage->header.size);
      break;
  }
}

UInt64 HyperVBalloon::getPhysicalMemorySizeInPages() {
  static UInt64 totalMemorySize = 0;
  static size_t totalMemorySizeSize = sizeof(totalMemorySize);
  if (totalMemorySize == 0) {
    sysctlbyname("hw.memsize", &totalMemorySize, &totalMemorySizeSize, nullptr, 0);
  }
  return totalMemorySize / PAGE_SIZE;
}

IOReturn HyperVBalloon::sendStatusReport(void*, void*, void*) {
  HyperVDynamicMemoryMessage message;
  memset(&message, 0, sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageStatusReport));
  message.header.type = kDynamicMemoryMessageTypeStatusReport;
  message.header.size = sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageStatusReport);
  OSIncrementAtomic(&_transactionId);
  message.header.transactionId = _transactionId;

  UInt64 totalPages = getPhysicalMemorySizeInPages();
  UInt64 committedPages, usingPages;
  getPagesStatus(&committedPages, &usingPages);
  // We just post two fields below, other fields are not needed by hypervisor
  message.statusReport.availablePages = totalPages - usingPages - kHyperVDynamicMemoryReservedPageCount;
  message.statusReport.committedPages = committedPages;

  HVDBGLOG("Posting memory status report: available %llu, committed %llu", message.statusReport.availablePages, message.statusReport.committedPages);

  // schedule next run
  _timerSource->setTimeoutMS(kHyperVDynamicMemoryStatusReportIntervalMilliseconds);
  
  return _hvDevice->writeInbandPacket(&message, sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageStatusReport), false);
}

void HyperVBalloon::getPagesStatus(UInt64 *committedPages, UInt64 *usingPages) {
  static mach_msg_type_name_t vmStatSize;
#if defined(__i386__)
  static vm_statistics_data_t vmStat;
  vmStatSize = HOST_VM_INFO_COUNT;
  host_statistics(host_self(), HOST_VM_INFO, (host_info_t) &vmStat, &vmStatSize);
#elif defined(__x86_64__)
  static vm_statistics64_data_t vmStat;
  vmStatSize = HOST_VM_INFO64_COUNT;
  host_statistics64(host_self(), HOST_VM_INFO64, (host_info64_t) &vmStat, &vmStatSize);
#else
#error Unsupported arch
#endif

  // wire_count:            "Wired Memory", pages that cannot be swapped out, e.g. kernel memory
  // internal_page_count:   "App Memory"
  // compressor_page_count: "Compressed Memory"
  // active_page_count:     pages that is actively used
  if (committedPages) {
    *committedPages = vmStat.wire_count + vmStat.internal_page_count + vmStat.compressor_page_count;
  }
  if (usingPages) {
    *usingPages = vmStat.wire_count + vmStat.active_count + vmStat.compressor_page_count;
  }
}

void HyperVBalloon::handleHotAddRequest(HyperVDynamicMemoryMessageHotAddRequest *request) {
  HyperVDynamicMemoryMessage message;
  memset(&message, 0, sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageHotAddResponse));
  message.header.type = kDynamicMemoryMessageTypeHotAddResponse;
  message.header.size = sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageHotAddResponse);
  OSIncrementAtomic(&_transactionId);
  message.header.transactionId = _transactionId;
  
  HVDBGLOG("Receiving hot add request");
  
  message.hotAddResponse.result = 1;    // success, meaning we processed the request successfully, so the host will not try to hot-add again
  message.hotAddResponse.pageCount = 0; // of course we don't support hot-add.
  
  HVDBGLOG("Sending hot add response");
  if (_hvDevice->writeInbandPacket(&message, sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageHotAddResponse), false) != kIOReturnSuccess) {
    HVSYSLOG("Hot add response send failed");
  }
}

void HyperVBalloon::handleInformationMessage(HyperVDynamicMemoryMessageInformation *info) {
  switch (info->type) {
    case kHyperVDynamicMemoryInformationTypeMaximumPageCount:
      if (info->dataSize == sizeof(UInt64)) {
        HVDBGLOG("Received information: maximum dynamic memory size is %lluMB", info->number >> (20 - PAGE_SHIFT));
      }
      break;
      
    default:
      HVDBGLOG("Received unknown information type %u", info->type);
      break;
  }
}

const OSSymbol* HyperVBalloon::pageFrameNumberToString(UInt64 pfn) {
  unsigned char buffer[7] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x0};
  // 40-bit PFN encodes into null-terminated string in which every char has 7 effective bits (highest bit masked with 1)
  buffer[0] |= pfn & 0x80;
  buffer[1] |= (pfn >> 7) & 0x80;
  buffer[2] |= (pfn >> 14) & 0x80;
  buffer[3] |= (pfn >> 21) & 0x80;
  buffer[4] |= (pfn >> 28) & 0x80;
  buffer[5] |= (pfn >> 35) & 0x80;
  buffer[6] = 0;
  return OSSymbol::withCString((const char *) buffer);
}

bool HyperVBalloon::inflateBalloon(UInt32 pageCount, bool morePages) {
  HVDBGLOG("This time inflate %lu pages and%s more", pageCount, morePages ? "" : " no");

  HyperVDynamicMemoryMessage *response = reinterpret_cast<HyperVDynamicMemoryMessage *>(_balloonInflationSendBuffer);
  memset(response, 0, kHyperVDynamicMemoryResponsePacketSize);

  // we fill size and transactionId just before sending
  response->header.type = kDynamicMemoryMessageTypeBalloonInflationResponse;

  // reset to 0 per every request
  response->inflationResponse.rangeCount = 0;
  response->inflationResponse.morePages = morePages;

  // We have to do one page per allocation
  // Hyper-V hypervisor will pass arbitrary fragmented memory block to guest which macOS guest need to release
  // while macOS doesn't support releasing part of a memory descriptor or splitting physical memory segment to indepedently relaseable parts
  for (int i = 0; i < pageCount; ++i) {
    // alloc memory in kernel page by page
    IOBufferMemoryDescriptor* chunk = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIOMemoryMapperNone, PAGE_SIZE, PAGE_SIZE);

    IOByteCount physicalSegmentSize;
    addr64_t physicalSegmentStart = chunk->getPhysicalSegment(0, &physicalSegmentSize, 0);
    HVDBGLOG("Allocated page: desc %p, raw %p, size %llu", chunk, physicalSegmentStart, physicalSegmentSize);
    
    // save (PFN, MemoryDescriptor) into map for further query
    const OSSymbol *key = pageFrameNumberToString(physicalSegmentStart >> PAGE_SHIFT);
    _pageFrameNumberToMemoryDescriptorMap->setObject(key, chunk);
    OSSafeReleaseNULL(chunk);
    OSSafeReleaseNULL(key);

    response->inflationResponse.ranges[i].startPageFrameNumber = physicalSegmentStart >> PAGE_SHIFT;
    response->inflationResponse.ranges[i].pageCount = 1;
    ++response->inflationResponse.rangeCount;
  }

  UInt32 responseLength = sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageBalloonInflationResponse) + sizeof(HyperVDynamicMemoryPageRange) * response->inflationResponse.rangeCount;
  response->header.size = responseLength;
  
  // size = protocol header + inflation response header + memory ranges
  IOReturn status;
  do {
    OSIncrementAtomic(&_transactionId);
    response->header.transactionId = _transactionId;
    HVDBGLOG("Send inflation request with %lu pages, length %u, transactionId %u", response->inflationResponse.rangeCount, responseLength, _transactionId);

    status = _hvDevice->writeInbandPacket(response, responseLength, false);
    if (status != kIOReturnSuccess) {
      HVDBGLOG("Send inflation request failed, code %d", status);
      IOSleep(20);
    }
  } while (status != kIOReturnSuccess);
  
  return true;
}

void HyperVBalloon::handleBalloonInflationRequest(HyperVDynamicMemoryMessageBalloonInflationRequest *request) {
  UInt32 pageCount = request->pageCount;
  UInt64 totalPages = getPhysicalMemorySizeInPages();
  UInt64 usedPages;
  getPagesStatus(nullptr, &usedPages);
  UInt64 availablePages = totalPages - usedPages;
  
  HVDBGLOG("Received request to inflate %lu pages", pageCount);

  if (availablePages < pageCount + kHyperVDynamicMemoryReservedPageCount) {
    HVSYSLOG("We don't have enough memory for balloon, filling a part of requested");
    pageCount = pageCount > kHyperVDynamicMemoryReservedPageCount ? (pageCount - kHyperVDynamicMemoryReservedPageCount) : 0;
  }

  while (pageCount > 0) {
    if (pageCount > kHyperVDynamicMemoryInflationChunkPageCount) {
      inflateBalloon(kHyperVDynamicMemoryInflationChunkPageCount, true);
      pageCount -= kHyperVDynamicMemoryInflationChunkPageCount;
    } else {
      inflateBalloon(pageCount, false);
      pageCount = 0;
    }
    break;
  }
}

void HyperVBalloon::handleBalloonDeflationRequest(HyperVDynamicMemoryMessageBalloonDeflationRequest *request) {
  for (int i = 0; i < request->rangeCount; ++i) {
    for (int j = 0; j < request->ranges[i].pageCount; ++j) {
      const OSSymbol *key = pageFrameNumberToString(request->ranges[i].startPageFrameNumber + j);
      IOBufferMemoryDescriptor* chunk = static_cast<IOBufferMemoryDescriptor*>(_pageFrameNumberToMemoryDescriptorMap->getObject(key));
      if (chunk) {
        chunk->complete();
        _pageFrameNumberToMemoryDescriptorMap->removeObject(key);
        HVDBGLOG("Memory block %llu released", request->ranges[i].startPageFrameNumber + j);
      } else {
        HVSYSLOG("Memory block %llu not found", request->ranges[i].startPageFrameNumber + j);
      }
      OSSafeReleaseNULL(key);
    }
  }

  // we should send response until the last deflation message received
  if (request->morePages) return;
  
  HyperVDynamicMemoryMessage message;
  memset(&message, 0, sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageBalloonDeflationResponse));
  message.header.type = kDynamicMemoryMessageTypeBalloonInflationResponse;
  message.header.size = sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageBalloonDeflationResponse);
  OSIncrementAtomic(&_transactionId);
  message.header.transactionId = _transactionId;
  
  HVDBGLOG("Sending deflation response");
  if (!_hvDevice->writeInbandPacket(&message, sizeof(HyperVDynamicMemoryMessageHeader) + sizeof(HyperVDynamicMemoryMessageBalloonDeflationResponse), false)) {
    HVSYSLOG("Deflation response send failed");
  }
}
