/** @file
  UDP4 test tool

  (C) Copyright 2016 Hewlett Packard Enterprise Development LP<BR>

  This software contains information confidential and proprietary to
  Hewlett Packard Enterprise. It shall not be reproduced in whole or in part,
  or transferred to other documents, or disclosed to third parties, or used
  for any purpose other than that for which it was obtained without the prior
  written consent of Hewlett Packard Enterprise.
  
**/

#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/NetLib.h>

//
// Consumed Protocols
//
#include <Protocol/ServiceBinding.h>
#include <Protocol/Dns4.h> //#include <Protocol/Udp4.h>


VOID
EFIAPI
DnsCommonNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if ((Event == NULL) || (Context == NULL)) {
    return ;
  }

  *((BOOLEAN *) Context) = TRUE;
}


/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.  
  @param[in] SystemTable    A pointer to the EFI System Table.
  
  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
Dns4TestMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS           Status;
  EFI_HANDLE           *HandleBuffer;
  UINTN                HandleNum;
  UINTN                Index;
  EFI_HANDLE           Dns4Child;//Udp4Child
  EFI_DNS4_PROTOCOL    *Dns4;//EFI_UDP4_PROTOCOL    *Udp4;
  EFI_SERVICE_BINDING_PROTOCOL  *Service;
  EFI_DNS4_CONFIG_DATA   Dns4CfgData;// EFI_UDP4_CONFIG_DATA          Udata;
  CHAR16 *HostName = L"www.lhjsjtu.com";
  EFI_DNS4_COMPLETION_TOKEN Token;
  BOOLEAN  IsDone;
  EFI_IPv4_ADDRESS    *IpAddress;
  //
  // Locate all the handles with DNS4 service binding protocol.
  //
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDns4ServiceBindingProtocolGuid,//gEfiUdp4ServiceBindingProtocolGuid
                  NULL,
                  &HandleNum,
                  &HandleBuffer
                 );
  if (EFI_ERROR (Status) || (HandleNum == 0)) {
    AsciiPrint ("%a: LocateHandles for DNS4SB failed, %r\n", __FUNCTION__, Status);
    return EFI_SUCCESS;
  }
  
  for (Index = 1; Index < 2; Index++) {
    AsciiPrint ("%a: Handle %d (%p):\n", __FUNCTION__, Index, HandleBuffer[Index]);
    //
    // Get the ServiceBinding Protocol
    //
    Status = gBS->OpenProtocol (
                    HandleBuffer[Index],
                    &gEfiDns4ServiceBindingProtocolGuid,
                    (VOID **) &Service,
                    ImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );

    if (EFI_ERROR (Status)) {
      AsciiPrint ("%a: DNS4SB GET_PROTOCOL failed %r\n", __FUNCTION__, Status);
      continue;
    }

    //
    // Create a child
    //
    Dns4Child = NULL;
    Status = Service->CreateChild (Service, &Dns4Child);
    if (EFI_ERROR (Status)) {
      AsciiPrint ("%a: DNS4 CreateChild failed %r\n", __FUNCTION__, Status);
      gBS->CloseProtocol (
             HandleBuffer[Index],
             &gEfiDns4ServiceBindingProtocolGuid,
             ImageHandle,
             NULL
             );
      continue;
    }

    //
    // Retrieve the DNS4 Protocol from handle
    //
    Status = gBS->OpenProtocol (
                  Dns4Child,
                  &gEfiDns4ProtocolGuid,
                  (VOID **) &Dns4,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
    if (EFI_ERROR (Status)) {
      AsciiPrint ("%a: DNS4 GET_PROTOCOL failed %r\n", __FUNCTION__, Status);
      Service->DestroyChild (Service, Dns4Child);
      gBS->CloseProtocol (
             HandleBuffer[Index],
             &gEfiDns4ServiceBindingProtocolGuid,
             ImageHandle,
             NULL
             );
      continue;
    }

	gBS->SetMem(&Dns4CfgData, sizeof(Dns4CfgData), 0);
	Dns4CfgData.DnsServerList = AllocatePool(sizeof (EFI_IPv4_ADDRESS));
	Dns4CfgData.DnsServerListCount =1;
	Dns4CfgData.DnsServerList->Addr[0] =192;
	Dns4CfgData.DnsServerList->Addr[1] =168;
	Dns4CfgData.DnsServerList->Addr[2] =10;
	Dns4CfgData.DnsServerList->Addr[3] =8;
	
	Dns4CfgData.UseDefaultSetting = TRUE;
	Dns4CfgData.EnableDnsCache = TRUE;
	Dns4CfgData.LocalPort = 0;
	Dns4CfgData.EnableDnsCache =TRUE;
	Dns4CfgData.Protocol = EFI_IP_PROTO_UDP;

	
	Status = Dns4->Configure(Dns4, &Dns4CfgData);
    if (EFI_ERROR (Status) && (Status != EFI_NO_MAPPING)) {
      return Status;
    }

	if (Status == EFI_NO_MAPPING) {
      AsciiPrint ("%a: DNS4 Configure failed %r\n", __FUNCTION__, Status);
      continue;
    }

    AsciiPrint ("%a: Successfully configured this DNS4 Handle!\n", __FUNCTION__);  
 // Create event to set the is done flag when name resolution is finished.
 //
    //initialize the Token:
    ZeroMem(&Token, sizeof(EFI_DNS4_COMPLETION_TOKEN));
	Status = gBS->CreateEvent (
					  EVT_NOTIFY_SIGNAL,
					  TPL_NOTIFY,
					  DnsCommonNotify,
					  &IsDone,
					  &Token.Event
					  );
	  if (EFI_ERROR (Status)) {
		goto Exit;
	  }
	// Start asynchronous name resolution.
   //
  Token.Status = EFI_NOT_READY;
  IsDone = FALSE;
  Status = Dns4->HostNameToIp (Dns4, HostName, &Token);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  while (!IsDone) {
    Dns4->Poll (Dns4);
  }
  IpAddress = AllocatePool(sizeof(EFI_IPv4_ADDRESS));
  // Name resolution is done, check result.
   Status = Token.Status;  
   if (!EFI_ERROR (Status)) {
	 if (Token.RspData.H2AData == NULL) {
	   Status = EFI_DEVICE_ERROR;
	   goto Exit;
	 }
	 if (Token.RspData.H2AData->IpCount == 0 || Token.RspData.H2AData->IpList == NULL) {
	   Status = EFI_DEVICE_ERROR;
	   goto Exit;
	 }
	 //
	 // We just return the first IP address from DNS protocol.
	 //
	 IP4_COPY_ADDRESS (IpAddress, Token.RspData.H2AData->IpList);
	 AsciiPrint ("IP Address is:%d.%d.%d.%d\n",

	 IpAddress->Addr[0],IpAddress->Addr[1],IpAddress->Addr[2],IpAddress->Addr[3]);  
	 AsciiPrint ("domain name is: %s\n",HostName);
	 Status = EFI_SUCCESS;
   }
    
  }

  Exit:
	
	if (Token.Event != NULL) {
	  gBS->CloseEvent (Token.Event);
	}
	if (Token.RspData.H2AData != NULL) {
	  if (Token.RspData.H2AData->IpList != NULL) {
		FreePool (Token.RspData.H2AData->IpList);
	  }
	  FreePool (Token.RspData.H2AData);
	}
  
	if (Dns4 != NULL) {
	  Dns4->Configure (Dns4, NULL);
	  
	  gBS->CloseProtocol (
           Dns4Child,
           &gEfiDns4ProtocolGuid,
           ImageHandle,
           NULL
           );
	}
  
	if (Dns4Child != NULL) {
	   Service->DestroyChild (Service, Dns4Child);
    gBS->CloseProtocol (
           HandleBuffer[Index],
           &gEfiDns4ServiceBindingProtocolGuid,
           ImageHandle,
           NULL
           );
	}
	
     AsciiPrint ("exit successfully\n");
	return Status;
  
}
	
	

