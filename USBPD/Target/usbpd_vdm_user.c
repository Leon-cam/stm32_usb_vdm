/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usbpd_vdm_user.c
  * @author  MCD Application Team
  * @brief   USBPD provider demo file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbpd_core.h"
#include "usbpd_dpm_conf.h"
#include "usbpd_vdm_user.h"
#include "usbpd_dpm_user.h"
/* USER CODE BEGIN Includes */
#include "string.h"
#include "main.h"
/* USER CODE END Includes */

/** @addtogroup STM32_USBPD_APPLICATION
  * @{
  */

/** @addtogroup STM32_USBPD_APPLICATION_VDM_USER
  * @{
  */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Private_define */
#define VDM_CUSTOM_SVID        0xFF01U

#define VDM_TRACE(_port, _msg) dbg_printf("[VDM] Port%d: %s\r\n", (_port), (_msg))
/* USER CODE END Private_define */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN Private_typedef */

/* USER CODE END Private_typedef */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Private_macro */

/* USER CODE END Private_macro */

/* Private function prototypes -----------------------------------------------*/
static USBPD_StatusTypeDef USBPD_VDM_DiscoverIdentity(uint8_t PortNum, USBPD_DiscoveryIdentity_TypeDef *pIdentity);
static USBPD_StatusTypeDef USBPD_VDM_DiscoverSVIDs(uint8_t PortNum, uint16_t **p_SVID_Info, uint8_t *nb);
static USBPD_StatusTypeDef USBPD_VDM_DiscoverModes(uint8_t PortNum, uint16_t SVID, uint32_t **p_ModeInfo, uint8_t *nbMode);
static USBPD_StatusTypeDef USBPD_VDM_ModeEnter(uint8_t PortNum, uint16_t SVID, uint32_t ModeIndex);
static USBPD_StatusTypeDef USBPD_VDM_ModeExit(uint8_t PortNum, uint16_t SVID, uint32_t ModeIndex);
static void USBPD_VDM_SendAttention(uint8_t PortNum, uint8_t *NbData, uint32_t *VDO);
static void USBPD_VDM_ReceiveAttention(uint8_t PortNum, uint8_t NbData, uint32_t VDO);
static USBPD_StatusTypeDef USBPD_VDM_ReceiveSpecific(uint8_t PortNum, USBPD_VDM_Command_Typedef VDMCommand, uint8_t *NbData, uint32_t *VDO);
static void USBPD_VDM_InformIdentity(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, USBPD_DiscoveryIdentity_TypeDef *pIdentity);
static void USBPD_VDM_InformSVID(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, USBPD_SVIDInfo_TypeDef *pListSVID);
static void USBPD_VDM_InformMode(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, USBPD_ModeInfo_TypeDef *pModesInfo);
static void USBPD_VDM_InformModeEnter(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, uint16_t SVID, uint32_t ModeIndex);
static void USBPD_VDM_InformModeExit(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, uint16_t SVID, uint32_t ModeIndex);
static void USBPD_VDM_InformSpecific(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_Command_Typedef VDMCommand, uint8_t *NbData, uint32_t *VDO);
static void USBPD_VDM_SendSpecific(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_Command_Typedef VDMCommand, uint8_t *NbData, uint32_t *VDO);
static void USBPD_VDM_SendUVDM(uint8_t PortNum, USBPD_UVDMHeader_TypeDef *pUVDM_Header, uint8_t *pNbData, uint32_t *pVDO);
static USBPD_StatusTypeDef USBPD_VDM_ReceiveUVDM(uint8_t PortNum, USBPD_UVDMHeader_TypeDef UVDM_Header, uint8_t *pNbData, uint32_t *pVDO);

/* USER CODE BEGIN Private_prototypes */

/* USER CODE END Private_prototypes */

/* Private variables ---------------------------------------------------------*/
USBPD_VDM_SettingsTypeDef DPM_VDM_Settings[USBPD_PORT_COUNT] =
{
  {
    .VDM_XID_SOP                = USBPD_XID,    /*!< A decimal number assigned by USB-IF before certification */
    .VDM_USB_VID_SOP            = USBPD_VID,    /*!< A decimal number assigned by USB-IF before certification */
    .VDM_PID_SOP                = USBPD_PID,    /*!< A unique number assigned by the Vendor ID holder identifying the product. */
    .VDM_ModalOperation         = MODAL_OPERATION_SUPPORTED, /*!< Product support Modes based on @ref USBPD_ModalOp_TypeDef */
    .VDM_bcdDevice_SOP          = 0xAAAA,        /*!< A unique number assigned by the Vendor ID holder containing identity information relevant to the release version of the product. */
    .VDM_USBHostSupport         = USB_NOTCAPABLE, /*!< Indicates whether the UUT is capable of enumerating USB Host */
    .VDM_USBDeviceSupport       = USB_NOTCAPABLE, /*!< Indicates whether the UUT is capable of enumerating USB Devices */
    .VDM_ProductTypeUFPorCP     = PRODUCT_TYPE_UNDEFINED, /*!< Product type UFP or CablePlug of the UUT based on @ref USBPD_ProductType_TypeDef */
  }
};

/* USER CODE BEGIN Private_variables */
static USBPD_DiscoveryIdentity_TypeDef sIdentity[USBPD_PORT_COUNT];
static uint16_t sUserSVIDs[1] = { VDM_CUSTOM_SVID };
static uint32_t sCustomModeVDO = 0x00000001U;
static uint8_t  sModeActive[USBPD_PORT_COUNT];
static uint16_t sApplePendingAction[USBPD_PORT_COUNT] = {0};

const USBPD_VDM_Callbacks vdmCallbacks =
{
  USBPD_VDM_DiscoverIdentity,
  USBPD_VDM_DiscoverSVIDs,
  USBPD_VDM_DiscoverModes,
  USBPD_VDM_ModeEnter,
  USBPD_VDM_ModeExit,
  USBPD_VDM_InformIdentity,
  USBPD_VDM_InformSVID,
  USBPD_VDM_InformMode,
  USBPD_VDM_InformModeEnter,
  USBPD_VDM_InformModeExit,
  USBPD_VDM_SendAttention,
  USBPD_VDM_ReceiveAttention,
  USBPD_VDM_SendSpecific,
  USBPD_VDM_ReceiveSpecific,
  USBPD_VDM_InformSpecific,
  USBPD_VDM_SendUVDM,
  USBPD_VDM_ReceiveUVDM,
};
/* USER CODE END Private_variables */

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  VDM Discovery identity callback
  * @note   Function is called to get Discovery identity information linked to the device and answer
  *         to SVDM Discovery identity init message sent by port partner
  * @param  PortNum current port number
  * @param  pIdentity Pointer on @ref USBPD_DiscoveryIdentity_TypeDef structure
  * @retval USBPD status: @ref USBPD_ACK or @ref USBPD_BUSY
  */
static USBPD_StatusTypeDef USBPD_VDM_DiscoverIdentity(uint8_t PortNum, USBPD_DiscoveryIdentity_TypeDef *pIdentity)
{
/* USER CODE BEGIN USBPD_VDM_DiscoverIdentity */
  pIdentity->IDHeader.b30.VID               = DPM_VDM_Settings[PortNum].VDM_USB_VID_SOP;
  pIdentity->IDHeader.b30.ModalOperation    = MODAL_OPERATION_SUPPORTED;
  pIdentity->IDHeader.b30.ProductTypeUFPorCP = PRODUCT_TYPE_AMA;
  pIdentity->IDHeader.b30.USBHostCapability = USB_NOTCAPABLE;
  pIdentity->IDHeader.b30.USBDevCapability  = USB_NOTCAPABLE;

  pIdentity->CertStatVDO.b.XID = DPM_VDM_Settings[PortNum].VDM_XID_SOP;
  pIdentity->ProductVDO.b.USBProductId = DPM_VDM_Settings[PortNum].VDM_PID_SOP;
  pIdentity->ProductVDO.b.bcdDevice    = DPM_VDM_Settings[PortNum].VDM_bcdDevice_SOP;

  VDM_TRACE(PortNum, "VDM:DiscoverIdentity ACK");
  return USBPD_ACK;
/* USER CODE END USBPD_VDM_DiscoverIdentity */
}

/**
  * @brief  VDM Discover SVID callback
  * @note   Function is called to retrieve SVID supported by device and answer
  *         to SVDM Discovery SVID init message sent by port partner
  * @param  PortNum        current port number
  * @param  p_SVID_Info Pointer on @ref USBPD_SVIDInfo_TypeDef structure
  * @param  pNbSVID     Pointer on number of SVID
  * @retval USBPD status  @ref USBPD_BUSY or @ref USBPD_ACK or @ref USBPD_NAK
  */
static USBPD_StatusTypeDef USBPD_VDM_DiscoverSVIDs(uint8_t PortNum, uint16_t **p_SVID_Info, uint8_t *pNbSVID)
{
/* USER CODE BEGIN USBPD_VDM_DiscoverSVIDs */
  *p_SVID_Info = sUserSVIDs;
  *pNbSVID     = 1;
  VDM_TRACE(PortNum, "VDM:DiscoverSVIDs ACK");
  return USBPD_ACK;
/* USER CODE END USBPD_VDM_DiscoverSVIDs */
}

/**
  * @brief  VDM Discover Mode callback (report all the modes supported by SVID)
  * @note   Function is called to report all the modes supported by selected SVID and answer
  *         to SVDM Discovery Mode init message sent by port partner
  * @param  PortNum current port number
  * @param  SVID         SVID value
  * @param  p_ModeTab    Pointer on the mode value
  * @param  NumberOfMode Number of mode available
  * @retval USBPD status
  */
static USBPD_StatusTypeDef USBPD_VDM_DiscoverModes(uint8_t PortNum, uint16_t SVID, uint32_t **p_ModeTab, uint8_t *NumberOfMode)
{
/* USER CODE BEGIN USBPD_VDM_DiscoverModes */
  if (SVID == VDM_CUSTOM_SVID)
  {
    *p_ModeTab   = &sCustomModeVDO;
    *NumberOfMode = 1;
    VDM_TRACE(PortNum, "VDM:DiscoverModes ACK");
    return USBPD_ACK;
  }
  return USBPD_NAK;
/* USER CODE END USBPD_VDM_DiscoverModes */
}

/**
  * @brief  VDM Mode enter callback
  * @note   Function is called to check if device can enter in the mode received for the selected SVID in the
  *         SVDM enter mode init message sent by port partner
  * @param  PortNum current port number
  * @param  SVID      SVID value
  * @param  ModeIndex Index of the mode to be entered
  * @retval USBPD status @ref USBPD_ACK/@ref USBPD_NAK
  */
static USBPD_StatusTypeDef USBPD_VDM_ModeEnter(uint8_t PortNum, uint16_t SVID, uint32_t ModeIndex)
{
/* USER CODE BEGIN USBPD_VDM_ModeEnter */
  if ((SVID == VDM_CUSTOM_SVID) && (ModeIndex == 1U))
  {
    sModeActive[PortNum] = 1U;
    VDM_TRACE(PortNum, "VDM:ModeEnter ACK");
    return USBPD_ACK;
  }
  return USBPD_NAK;
/* USER CODE END USBPD_VDM_ModeEnter */
}

/**
  * @brief  VDM Mode exit callback
  * @note   Function is called to check if device can exit from the mode received for the selected SVID in the
  *         SVDM exit mode init message sent by port partner
  * @param  PortNum current port number
  * @param  SVID      SVID value
  * @param  ModeIndex Index of the mode to be exited
  * @retval USBPD status @ref USBPD_ACK/@ref USBPD_NAK
  */
static USBPD_StatusTypeDef USBPD_VDM_ModeExit(uint8_t PortNum, uint16_t SVID, uint32_t ModeIndex)
{
/* USER CODE BEGIN USBPD_VDM_ModeExit */
  if ((SVID == VDM_CUSTOM_SVID) && (ModeIndex == 1U))
  {
    sModeActive[PortNum] = 0U;
    VDM_TRACE(PortNum, "VDM:ModeExit ACK");
    return USBPD_ACK;
  }
  return USBPD_NAK;
/* USER CODE END USBPD_VDM_ModeExit */
}

/**
  * @brief  Send VDM Attention message callback
  * @note   Function is called when device wants to send a SVDM attention message to port partner
  *         (for instance DP status can be filled through this function)
  * @param  PortNum    current port number
  * @param  pNbData    Pointer of number of VDO to send
  * @param  pVDO       Pointer of VDO to send
  * @retval None
  */
static void USBPD_VDM_SendAttention(uint8_t PortNum, uint8_t *pNbData, uint32_t *pVDO)
{
/* USER CODE BEGIN USBPD_VDM_SendAttention */

/* USER CODE END USBPD_VDM_SendAttention */
}

/**
  * @brief  Receive VDM Attention callback
  * @note   Function is called when a SVDM attention init message has been received from port partner
  *         (for instance, save DP status data through this function)
  * @param  PortNum   current port number
  * @param  NbData    Number of received VDO
  * @param  VDO       Received VDO
  * @retval None
  */
static void USBPD_VDM_ReceiveAttention(uint8_t PortNum, uint8_t NbData, uint32_t VDO)
{
/* USER CODE BEGIN USBPD_VDM_ReceiveAttention */

/* USER CODE END USBPD_VDM_ReceiveAttention */
}

/**
  * @brief  VDM Receive Specific message callback
  * @note   Function is called to answer to a SVDM specific init message received by port partner.
  *         (for instance, retrieve DP status or DP configure data through this function)
  * @param  PortNum     Current port number
  * @param  VDMCommand  VDM command based on @ref USBPD_VDM_Command_Typedef
  * @param  pNbData     Pointer of number of received VDO and used for the answer
  * @param  pVDO        Pointer of received VDO and use for the answer
  * @retval USBPD Status
  */
static USBPD_StatusTypeDef USBPD_VDM_ReceiveSpecific(uint8_t PortNum, USBPD_VDM_Command_Typedef VDMCommand, uint8_t *pNbData, uint32_t *pVDO)
{
/* USER CODE BEGIN USBPD_VDM_ReceiveSpecific */
  return USBPD_NAK;
/* USER CODE END USBPD_VDM_ReceiveSpecific */
}

/**
  * @brief  Inform identity callback
  * @note   Function is called to save Identity information received in Discovery identity from port partner
            (answer to SVDM discovery identity sent by device)
  * @param  PortNum       current port number
  * @param  SOPType       SOP type
  * @param  CommandStatus Command status based on @ref USBPD_VDM_CommandType_Typedef
  * @param  pIdentity     Pointer on the discovery identity information based on @ref USBPD_DiscoveryIdentity_TypeDef
  * @retval None
*/
static void USBPD_VDM_InformIdentity(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, USBPD_DiscoveryIdentity_TypeDef *pIdentity)
{
/* USER CODE BEGIN USBPD_VDM_InformIdentity */
  switch(CommandStatus)
  {
  case SVDM_RESPONDER_ACK :
    sIdentity[PortNum] = *pIdentity;
    dbg_printf("[VDM] Port%d: InformIdentity ACK VID=0x%04lX\r\n", PortNum,
           (unsigned long)(pIdentity->IDHeader.b30.VID));
    USBPD_PE_SVDM_RequestSVID(PortNum, USBPD_SOPTYPE_SOP);
    break;
  case SVDM_RESPONDER_NAK :
    VDM_TRACE(PortNum, "VDM:InformId NAK");
    break;
  case SVDM_RESPONDER_BUSY :
    VDM_TRACE(PortNum, "VDM:InformId BUSY");
    break;
  default :
    break;
  }
/* USER CODE END USBPD_VDM_InformIdentity */
}

/**
  * @brief  Inform SVID callback
  * @note   Function is called to save list of SVID received in Discovery SVID from port partner
            (answer to SVDM discovery SVID sent by device)
  * @param  PortNum       current port number
  * @param  SOPType       SOP type
  * @param  CommandStatus Command status based on @ref USBPD_VDM_CommandType_Typedef
  * @param  pListSVID     Pointer of list of SVID based on @ref USBPD_SVIDInfo_TypeDef
  * @retval None
  */
static void USBPD_VDM_InformSVID(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, USBPD_SVIDInfo_TypeDef *pListSVID)
{
/* USER CODE BEGIN USBPD_VDM_InformSVID */
  switch(CommandStatus)
  {
  case SVDM_RESPONDER_ACK :
    if (pListSVID->NumSVIDs > 0u)
    {
      dbg_printf("[VDM] Port%d: InformSVID ACK count=%d SVID[0]=0x%04X\r\n",
             PortNum, pListSVID->NumSVIDs, pListSVID->SVIDs[0]);
      USBPD_PE_SVDM_RequestMode(PortNum, USBPD_SOPTYPE_SOP, pListSVID->SVIDs[0]);
    }
    break;
  case SVDM_RESPONDER_NAK :
    dbg_printf("[VDM] Port%d: InformSVID NAK\r\n", PortNum);
    break;
  case SVDM_RESPONDER_BUSY :
    dbg_printf("[VDM] Port%d: InformSVID BUSY\r\n", PortNum);
    break;
  default :
    break;
  }
/* USER CODE END USBPD_VDM_InformSVID */
}

/**
  * @brief  Inform Mode callback ( received in Discovery Modes ACK)
  * @note   Function is called to save list of modes linked to SVID received in Discovery mode from port partner
            (answer to SVDM discovery mode sent by device)
  * @param  PortNum         current port number
  * @param  SOPType         SOP type
  * @param  CommandStatus   Command status based on @ref USBPD_VDM_CommandType_Typedef
  * @param  pModesInfo      Pointer of Modes info based on @ref USBPD_ModeInfo_TypeDef
  * @retval None
  */
static void USBPD_VDM_InformMode(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, USBPD_ModeInfo_TypeDef *pModesInfo)
{
/* USER CODE BEGIN USBPD_VDM_InformMode */
  switch(CommandStatus)
  {
  case SVDM_RESPONDER_ACK :
    if (pModesInfo->NumModes > 0u)
    {
      VDM_TRACE(PortNum, "VDM:InformMode ACK -> EnterMode");
      USBPD_PE_SVDM_RequestModeEnter(PortNum, USBPD_SOPTYPE_SOP, pModesInfo->SVID, 1u);
    }
    break;
  case SVDM_RESPONDER_NAK :
    VDM_TRACE(PortNum, "VDM:InformMode NAK");
    break;
  case SVDM_RESPONDER_BUSY :
    VDM_TRACE(PortNum, "VDM:InformMode BUSY");
    break;
  default :
    break;
  }
/* USER CODE END USBPD_VDM_InformMode */
}

/**
  * @brief  Inform Mode enter callback
  * @note   Function is called to inform if port partner accepted or not to enter in the mode
  *         specified in the SVDM enter mode sent by the device
  * @param  PortNum   current port number
  * @param  SOPType       SOP type
  * @param  CommandStatus Command status based on @ref USBPD_VDM_CommandType_Typedef
  * @param  SVID      SVID ID
  * @param  ModeIndex Index of the mode to be entered
  * @retval None
  */
static void USBPD_VDM_InformModeEnter(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, uint16_t SVID, uint32_t ModeIndex)
{
/* USER CODE BEGIN USBPD_VDM_InformModeEnter */
  switch(CommandStatus)
  {
  case SVDM_RESPONDER_ACK :
    sModeActive[PortNum] = 1U;
    VDM_TRACE(PortNum, "VDM:ModeEnter confirmed");
    break;
  case SVDM_RESPONDER_NAK :
    VDM_TRACE(PortNum, "VDM:ModeEnter NAK");
    break;
  case SVDM_RESPONDER_BUSY :
    VDM_TRACE(PortNum, "VDM:ModeEnter BUSY");
    break;
  default :
    break;
  }
/* USER CODE END USBPD_VDM_InformModeEnter */
}

/**
  * @brief  Inform Exit enter callback
  * @param  PortNum   current port number
  * @param  SVID      SVID ID
  * @param  ModeIndex Index of the mode to be entered
  * @retval None
  */
static void USBPD_VDM_InformModeExit(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_CommandType_Typedef CommandStatus, uint16_t SVID, uint32_t ModeIndex)
{
/* USER CODE BEGIN USBPD_VDM_InformModeExit */
  switch(CommandStatus)
  {
  case SVDM_RESPONDER_ACK :
    sModeActive[PortNum] = 0U;
    VDM_TRACE(PortNum, "VDM:ModeExit confirmed");
    break;
  case SVDM_RESPONDER_NAK :
    VDM_TRACE(PortNum, "VDM:ModeExit NAK");
    break;
  case SVDM_RESPONDER_BUSY :
    VDM_TRACE(PortNum, "VDM:ModeExit BUSY");
    break;
  default :
    break;
  }
/* USER CODE END USBPD_VDM_InformModeExit */
}

/**
  * @brief  VDM Send Specific message callback
  * @note   Function is called when device wants to send a SVDM specific init message to port partner
  *         (for instance DP status or DP configure can be filled through this function)
  * @param  PortNum    current port number
  * @param  SOPType    SOP type
  * @param  VDMCommand VDM command based on @ref USBPD_VDM_Command_Typedef
  * @param  pNbData    Pointer of number of VDO to send
  * @param  pVDO       Pointer of VDO to send
  * @retval None
  */
static void USBPD_VDM_SendSpecific(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_Command_Typedef VDMCommand, uint8_t *pNbData, uint32_t *pVDO)
{
/* USER CODE BEGIN USBPD_VDM_SendSpecific */
  dbg_printf("[VDM] Port%d: SendSpecific SOP=%d CMD=0x%lx\r\n", PortNum, (int)SOPType, (unsigned long)VDMCommand);
  if ((SOPType == USBPD_SOPTYPE_SOP) && (VDMCommand == APPLE_CMD_PERFORM_ACTION))
  {
    *pNbData = 2;
    /* VDO1: Action ID (lower 16 bits), other bits are mask/flags */
    pVDO[0] = (uint32_t)sApplePendingAction[PortNum];
    /* VDO2: Argument (0x8000 for reboot, 0x8001 for DFU) - based on research */
    if (sApplePendingAction[PortNum] == APPLE_ACTION_REBOOT) {
      pVDO[1] = 0x80000000U;
    } else if (sApplePendingAction[PortNum] == APPLE_ACTION_DFU) {
      pVDO[1] = 0x80010000U;
    } else {
      pVDO[1] = 0x00000000U;
    }
    sApplePendingAction[PortNum] = 0; /* Clear pending action */
  }
/* USER CODE END USBPD_VDM_SendSpecific */
}

/**
  * @brief  VDM Specific message callback to inform user of reception of VDM specific message
  * @note   Function is called when answer from SVDM specific init message has been received by the device
  *         (for instance, save DP status and DP configure data through this function)
  * @param  PortNum    current port number
  * @param  SOPType    SOP type
  * @param  VDMCommand VDM command based on @ref USBPD_VDM_Command_Typedef
  * @param  pNbData    Pointer of number of received VDO
  * @param  pVDO       Pointer of received VDO
  * @retval None
  */
static void USBPD_VDM_InformSpecific(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, USBPD_VDM_Command_Typedef VDMCommand, uint8_t *pNbData, uint32_t *pVDO)
{
/* USER CODE BEGIN USBPD_VDM_InformSpecific */
  dbg_printf("[VDM] Port%d: InformSpecific SOP=%d CMD=0x%lx Nb=%d VDO0=0x%lx\r\n", 
             PortNum, (int)SOPType, (unsigned long)VDMCommand, (int)*pNbData, (unsigned long)pVDO[0]);
/* USER CODE END USBPD_VDM_InformSpecific */
}

/**
  * @brief  VDM Send Unstructured message callback
  * @note   Aim of this function is to fill the UVDM message which contains 1 VDM Header + 6 VDO
  *         This callback will be called when user requests to send a UVDM message thanks
  *         to USBPD_DPM_RequestUVDMMessage function
  * @param  PortNum       current port number
  * @param  pUVDM_Header  Pointer on UVDM header based on @ref USBPD_UVDMHeader_TypeDef
  * @param  pNbData       Pointer of number of VDO to send (max size must be equal to 6)
  * @param  pVDO          Pointer of VDO to send (up to 6 x uint32_t)
  * @retval None
  */
static void USBPD_VDM_SendUVDM(uint8_t PortNum, USBPD_UVDMHeader_TypeDef *pUVDM_Header, uint8_t *pNbData, uint32_t *pVDO)
{
/* USER CODE BEGIN USBPD_VDM_SendUVDM */
  pUVDM_Header->b.VID = DPM_VDM_Settings[PortNum].VDM_USB_VID_SOP;
  *pNbData = 2;
  pVDO[0] = 0xDEAD0001U;
  pVDO[1] = 0xBEEF0002U;
  VDM_TRACE(PortNum, "VDM:SendUVDM");
/* USER CODE END USBPD_VDM_SendUVDM */
}

/**
  * @brief  Unstructured VDM  message callback to inform user of reception of UVDM message
  * @param  PortNum    current port number
  * @param  UVDM_Header UVDM header based on @ref USBPD_UVDMHeader_TypeDef
  * @param  pNbData    Pointer of number of received VDO
  * @param  pVDO       Pointer of received VDO
  * @retval USBPD Status
  */
static USBPD_StatusTypeDef USBPD_VDM_ReceiveUVDM(uint8_t PortNum, USBPD_UVDMHeader_TypeDef UVDM_Header, uint8_t *pNbData, uint32_t *pVDO)
{
/* USER CODE BEGIN USBPD_VDM_ReceiveUVDM */
  dbg_printf("[VDM] Port%d: RxUVDM VID=0x%04X NbData=%d\r\n",
         PortNum, UVDM_Header.b.VID, *pNbData);
  for (uint8_t i = 0; i < *pNbData; i++)
  {
    dbg_printf("  VDO[%d] = 0x%08lX\r\n", i, (unsigned long)pVDO[i]);
  }
  return USBPD_ACK;
/* USER CODE END USBPD_VDM_ReceiveUVDM */
}

/* USER CODE BEGIN Private_functions */

/* USER CODE END Private_functions */

/* Exported functions ---------------------------------------------------------*/
/**
  * @brief  VDM Initialization function
  * @param  PortNum     Index of current used port
  * @retval status
  */
USBPD_StatusTypeDef USBPD_VDM_UserInit(uint8_t PortNum)
{
/* USER CODE BEGIN USBPD_VDM_UserInit */
  USBPD_PE_InitVDM_Callback(PortNum, (USBPD_VDM_Callbacks *)&vdmCallbacks);
  memset(&sIdentity[PortNum], 0, sizeof(USBPD_DiscoveryIdentity_TypeDef));
  sModeActive[PortNum] = 0U;
  VDM_TRACE(PortNum, "VDM:UserInit OK");
  return USBPD_OK;
/* USER CODE END USBPD_VDM_UserInit */
}

/**
  * @brief  VDM Reset function
  * @param  PortNum     Index of current used port
  * @retval status
  */
void USBPD_VDM_UserReset(uint8_t PortNum)
{
/* USER CODE BEGIN USBPD_VDM_UserReset */

/* USER CODE END USBPD_VDM_UserReset */
}

/* USER CODE BEGIN Exported_functions */
void USBPD_VDM_StartDiscovery(uint8_t PortNum)
{
  VDM_TRACE(PortNum, "VDM:StartDiscovery->Id");
  USBPD_PE_SVDM_RequestIdentity(PortNum, USBPD_SOPTYPE_SOP);
}

void USBPD_VDM_SendAppleCommand(uint8_t PortNum, uint16_t Action)
{
  sApplePendingAction[PortNum] = Action;
  dbg_printf("[VDM] Port%d: Sending Apple Action 0x%04X\r\n", PortNum, Action);

  /* Apple proprietary VDMs are sent over SOP (to the port partner), not SOP'Debug/SOP''Debug */
  USBPD_StatusTypeDef status = USBPD_PE_SVDM_RequestSpecific(PortNum, USBPD_SOPTYPE_SOP, APPLE_CMD_PERFORM_ACTION, APPLE_SVID);
  dbg_printf("[VDM] Port%d: RequestSpecific (SOP) returned %d\r\n", PortNum, (int)status);
}
/* USER CODE END Exported_functions */

/**
  * @}
  */

/**
  * @}
  */

