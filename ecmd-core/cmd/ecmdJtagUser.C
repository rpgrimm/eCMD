//IBM_PROLOG_BEGIN_TAG
/* 
 * Copyright 2003,2016 IBM International Business Machines Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//IBM_PROLOG_END_TAG


//----------------------------------------------------------------------
//  Includes
//----------------------------------------------------------------------
#include <stdio.h>
#include <string.h>

#include <ecmdCommandUtils.H>
#include <ecmdReturnCodes.H>
#include <ecmdClientCapi.H>
#include <ecmdUtils.H>
#include <ecmdDataBuffer.H>
#include <ecmdSharedUtils.H>

//----------------------------------------------------------------------
//  User Types
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//  Constants
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//  Macros
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//  Internal Function Prototypes
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//  Global Variables
//----------------------------------------------------------------------

//---------------------------------------------------------------------
// Member Function Specifications
//---------------------------------------------------------------------


uint32_t ecmdSendCmdUser(int argc, char * argv[]) {
  uint32_t rc = ECMD_SUCCESS;
  uint32_t coeRc = ECMD_SUCCESS;                                    //@01
  /*  bool expectFlag = false; */
  /*  bool xstateFlag = false; */
  ecmdLooperData looperdata;            ///< Store internal Looper data
  ecmdChipTarget target;                ///< Current target being operated on
  uint32_t instruction;                 ///< Instruction to send to chip
  uint32_t modifier;                    ///< Modifier to send to chip
  ecmdDataBuffer statusBuffer;          ///< Buffer to hold return status data
  bool validPosFound = false;           ///< Did the looper find something to run on?

  /************************************************************************/
  /* Parse Local FLAGS here!                                              */
  /************************************************************************/
  /* get format flag, if it's there */
  std::string format;
  char * formatPtr = ecmdParseOptionWithArgs(&argc, &argv, "-o");
  if (formatPtr == NULL) {
    format = "x";
  }
  else {
    format = formatPtr;
  }
  //Check verbose option
  bool verbose = ecmdParseOption(&argc, &argv, "-v");

  /************************************************************************/
  /* Parse Common Cmdline Args                                            */
  /************************************************************************/

  rc = ecmdCommandArgs(&argc, &argv);
  if (rc) return rc;

//@01
  /* Global args have been parsed, we can read if -coe was given */
  bool coeMode = ecmdGetGlobalVar(ECMD_GLOBALVAR_COEMODE); ///< Are we in continue on error mode

  /************************************************************************/
  /* Parse Local ARGS here!                                               */
  /************************************************************************/
  if (argc < 3) {  //chip + instruction + modifier
    ecmdOutputError("sendcmd - Too few arguments specified; you need at least a chip, instruction, and modifier.\n");
    ecmdOutputError("sendcmd - Type 'sendcmd -h' for usage.\n");
    return ECMD_INVALID_ARGS;
  }

  //Setup the target that will be used to query the system config 
  std::string chipType, chipUnitType;
  ecmdParseChipField(argv[0], chipType, chipUnitType);
  if (chipUnitType != "") {
    ecmdOutputError("sendcmd - chipUnit specified on the command line, this function doesn't support chipUnits.\n");
    return ECMD_INVALID_ARGS;
  }
  target.chipType = chipType;
  target.chipTypeState = ECMD_TARGET_FIELD_VALID;
  target.cageState = target.nodeState = target.slotState = target.posState = ECMD_TARGET_FIELD_WILDCARD;
  target.chipUnitTypeState = target.chipUnitNumState = target.threadState = ECMD_TARGET_FIELD_UNUSED;

  // we need the instruction and modifier

  if (strlen(argv[1]) > 2) {
    ecmdOutputError("sendcmd - The instruction has to be <= 8 bits\n");
    return ECMD_INVALID_ARGS;
  } else if (!ecmdIsAllHex(argv[1])) {
    ecmdOutputError("sendcmd - Instruction data contained some non-hex characters\n");
    return ECMD_INVALID_ARGS;
  }
  ecmdGenB32FromHexRight(&instruction, argv[1], 32);

  if (strlen(argv[2]) > 8) { 
    ecmdOutputError("sendcmd - The modifier has to be <= 24 bits\n");
    return ECMD_INVALID_ARGS;
  } else if (!ecmdIsAllHex(argv[2])) {
    ecmdOutputError("sendcmd - Modifier data contained some non-hex characters\n");
    return ECMD_INVALID_ARGS;
  }
  ecmdGenB32FromHexRight(&modifier, argv[2], 32);


  if (argc > 3) {
    ecmdOutputError("sendcmd - Too many arguments specified; you probably added an option that wasn't recognized.\n");
    ecmdOutputError("sendcmd - Type 'sendcmd -h' for usage.\n");
    return ECMD_INVALID_ARGS;
  }

  /************************************************************************/
  /* Kickoff Looping Stuff                                                */
  /************************************************************************/

  rc = ecmdLooperInit(target, ECMD_SELECTED_TARGETS_LOOP, looperdata);
  if (rc) return rc;
  /*  char outstr[30]; */
  std::string printed;

  while (ecmdLooperNext(target, looperdata) && (!coeRc || coeMode)) {    //@01

    rc = sendCmd(target, instruction, modifier, statusBuffer);
    if (rc) {
      printed = "sendcmd - Error occured performing sendcmd on ";
      printed += ecmdWriteTarget(target) + "\n";
      ecmdOutputError( printed.c_str() );
      coeRc = rc;                                   //@01                       
      continue;                                     //@01 
    }
    else {
      validPosFound = true;     
    }


    printed = ecmdWriteTarget(target) + "  ";
    std::string dataStr = ecmdWriteDataFormatted(statusBuffer, format);
    printed += dataStr;
    ecmdOutput( printed.c_str() ); 

    if ( verbose ) {

      ecmdChipData chipdata;
      rc = ecmdGetChipData (target, chipdata);

      if ( (!rc) && (chipdata.interfaceType == ECMD_INTERFACE_CFAM) ) {
        printed = "\n\t\tInstruction Status Register\n";
        printed += "\t\t---------------------------\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(0, 1) + " Attention Active" + "\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(1, 1) + " Checkstop" + "\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(2, 1) + " Special Attention" + "\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(3, 1) + " Recoverable Error" + "\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(4, 1) + " SCOM Attention" + "\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(5, 1) + " CRC Miscompare on previous data scan-in" + "\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(6, 1) + " Invalid Instruction" + "\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(7, 1) + " PGOOD Indicator(set to '1' by flush, set to '0' by first JTAG instruction)" + "\n";
        printed += "\t\t " + statusBuffer.genHexLeftStr(8, 4) + " JTAG Instruction count(Incremented following Shift-IR) Bits(8:11). (Hex Left)" + "\n";

        printed += "\t\t " + statusBuffer.genHexRightStr(12, 1) + " Data scan occured after the last instruction" + "\n";
        printed += "\t\t " + statusBuffer.genHexLeftStr(13, 3) + " Reserved Bits(13:15). (Hex Left)" + "\n";

        printed += "\t      " + statusBuffer.genHexLeftStr(16,14) + " Clock States(1 = running) Bits(16:29). (Hex Left)" + "\n";

        printed += "\t\t " + statusBuffer.genHexRightStr(30, 1) + " IEEE defined 0"  + "\n";
        printed += "\t\t " + statusBuffer.genHexRightStr(31, 1) + " IEEE defined 1"  + "\n";
        ecmdOutput(printed.c_str());
      }
      else if (rc) {
        printed = "sendcmd - Error occured performing chipinfo query on ";
        printed += ecmdWriteTarget(target) + "\n";
        ecmdOutputError( printed.c_str() );
        coeRc = rc;                                   //@01                       
        continue;                                     //@01
      }

    }
  }
  // coeRc will be the return code from in the loop, coe mode or not.
  if (coeRc) return coeRc;

  if (!validPosFound) {
    ecmdOutputError("Unable to find a valid chip to execute command on\n");
    return ECMD_TARGET_NOT_CONFIGURED;
  }

  return rc;
}
