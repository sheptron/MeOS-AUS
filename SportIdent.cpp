/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2016 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsv�gen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

// SportIdent.cpp: implementation of the SportIdent class.
//
//////////////////////////////////////////////////////////////////////

// Check implementation r56.

#include "stdafx.h"
#include "meos.h"
#include "SportIdent.h"
#include <fstream>
#include <process.h>
#include <winsock2.h>
#include "localizer.h"
#include "meos_util.h"
#include "oPunch.h"
#include <algorithm>

#include <iostream>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//#define DEBUG_SI

SI_StationInfo::SI_StationInfo()
{
  ThreadHandle=0;
  hComm=0;
  memset(&TimeOuts, 0, sizeof(TimeOuts));

  StationNumber=0;
  StationMode=0;
  Extended=false;
  HandShake=false;
  AutoSend=false;

  tcpPort=0;
  localZeroTime=0;
}

SportIdent::SportIdent(HWND hWnd, DWORD Id)
{
  ClassId=Id;
  hWndNotify=hWnd;

  //hComm=0;
  //ThreadHandle=0;
  n_SI_Info=0;

  ZeroTime=0;
  InitializeCriticalSection(&SyncObj);

  tcpPortOpen=0;
  serverSocket=0;
}

SportIdent::~SportIdent()
{
  CloseCom(0);
  DeleteCriticalSection(&SyncObj);
}

// CRC algoritm used by SPORTIdent GmbH
// implemented for MeOS in C++
WORD SportIdent::CalcCRC(BYTE *data, DWORD count)
{
  // Return 0 for no or one data byte
  if (count<2)
    return 0;

  size_t index=0;
  WORD crc = (WORD(data[index])<<8) + WORD(data[index+1]);
  index +=2;
  // Return crc for two data bytes
  if (count==2)
    return crc;

  WORD value;
  for (size_t k = count>>1; k>0; k--) {
    if (k>1) {
      value = (WORD(data[index])<<8) + WORD(data[index+1]);
      index +=2;
    }
    else  // If the number of bytes is odd, complete with 0.
      value = (count&1) ? data[index]<<8 : 0;

    for (int j = 0; j<16; j++) {
      if (crc & 0x8000) {
        crc  <<= 1;
        if (value & 0x8000)
          crc++;
        crc ^= 0x8005;
      }
      else {
        crc  <<= 1;
        if (value & 0x8000)
          crc++;
      }
      value <<= 1;
    }
  }
  return crc;
}

void SportIdent::SetCRC(BYTE *bf)
{
  DWORD len=bf[1];
  WORD crc=CalcCRC(bf, len+2);
  bf[len+2]=HIBYTE(crc);
  bf[len+3]=LOBYTE(crc);
}

bool SportIdent::CheckCRC(BYTE *bf)
{
  DWORD len=bf[1];
  WORD crc=CalcCRC(bf, len+2);

  return bf[len+2]==HIBYTE(crc) && bf[len+3]==LOBYTE(crc);
}

bool SportIdent::ReadSystemData(SI_StationInfo *si, int retry)
{
  BYTE c[16];
  BYTE buff[32];

  c[0]=STX;
  c[1]=0x83;
  c[2]=0x02;
  c[3]=0x70; //Request address 0x70
  c[4]=0x06; //And 6 bytes
  //Address 0x74 = protocoll settings
  //Address 0x71 = Programming ctrl/readout/finish etc.
  SetCRC(c+1);
  c[7]=ETX;

  DWORD written=0;
  WriteFile(si->hComm, c, 8, &written, NULL);
  Sleep(100);
  memset((void *)buff, 0, 30);
  //if (ReadBytes_delay(buff, 10, si->hComm)==10){
  ReadBytes_delay(buff, 15, si->hComm);//==10){
  if (1){
    if (CheckCRC(LPBYTE(buff+1))){
      si->StationNumber=MAKEWORD(buff[4], buff[3]);
      BYTE PR=buff[6+4];
      BYTE MO=buff[6+1];
      si->Extended=(PR&0x1)!=0;
      si->HandShake=(PR&0x4)!=0;
      si->AutoSend=(PR&0x2)!=0;
      si->StationMode=MO & 0xf;
    }
    else if (retry>0)
      return ReadSystemData(si, retry-1);
    else return false;
  }
  else if (retry>0)
    return ReadSystemData(si, retry-1);
  else
    return false;

  return true;
}

string decode(BYTE *bf, int read)
{
  string st;
  for(int k=0;k<=read;k++){
    if (bf[k]==STX)
      st+="STX ";
    else if (bf[k]==ETX)
      st+="ETX ";
    else if (bf[k]==ACK)
      st+="ACK ";
    else if (bf[k]==DLE)
      st+="DLE ";
    else{
      char d[10];
      sprintf_s(d, "%02X  ", bf[k]);
      st+=d;
    }
  }
  return st;
}

bool SportIdent::ReadSystemDataOldProtocoll(SI_StationInfo *si, int retry)
{
  return false;
  BYTE c[16];
  BYTE buff[32];

  c[0]=STX;
  //[1]=0x83;
  //c[2]=0x02;
  //c[3]=0x72; //Request address 0x70
  //c[4]=0x01; //And 6 bytes
  //Address 0x74 = protocoll settings
  //Address 0x71 = Programming ctrl/readout/finish etc.
  SetCRC(c+1);
  c[7]=ETX;

  DWORD written=0;
  WriteFile(si->hComm, c, 8, &written, NULL);
  Sleep(100);
  memset((void *)buff, 0, 30);
  //if (ReadBytes_delay(buff, 10, si->hComm)==10){
  int read=ReadBytesDLE(buff, 26, si->hComm);//==10){
  if (1){

    string code=decode(buff, read);

    //if (CheckCRC(LPBYTE(buff+1))){
      si->StationNumber=buff[4];
      BYTE PR=buff[6+4];
      BYTE MO=buff[6+1];
      si->Extended=(PR&0x1)!=0;
      si->HandShake=(PR&0x4)!=0;
      si->AutoSend=(PR&0x2)!=0;
      si->StationMode=MO & 0xf;
  }
  else if (retry>0)
    return ReadSystemDataOldProtocoll(si, retry-1);
  else
    return false;

  return true;
}

bool SportIdent::OpenComListen(const char *com, DWORD BaudRate) {
  CloseCom(com);

  SI_StationInfo *si = findStation(com);

  if (!si) {
    SI_Info[n_SI_Info].ComPort=com;
    SI_Info[n_SI_Info].ThreadHandle=0;
    si=&SI_Info[n_SI_Info];
    n_SI_Info++;
  }

  si->StationNumber=0;
  si->Extended=0;
  si->HandShake=0;
  si->StationMode=0;

  string comfile=string("//./")+com;
  si->hComm = CreateFile( comfile.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            0,
            OPEN_EXISTING,
            0,
            0);

  if (si->hComm == INVALID_HANDLE_VALUE) {
    si->hComm=0;
    return false;   // error opening port; abort
  }

  //Store comport
  //ComPort=com;

  GetCommTimeouts(si->hComm, &si->TimeOuts);
  COMMTIMEOUTS MyTimeOuts=si->TimeOuts;
  MyTimeOuts.ReadIntervalTimeout=50;
  MyTimeOuts.ReadTotalTimeoutMultiplier = 10;
  MyTimeOuts.ReadTotalTimeoutConstant = 300;
  MyTimeOuts.WriteTotalTimeoutMultiplier = 10;
  MyTimeOuts.WriteTotalTimeoutConstant = 300;

  SetCommTimeouts(si->hComm, &MyTimeOuts);

  DCB dcb;
  memset(&dcb, 0, sizeof(dcb));
  dcb.DCBlength=sizeof(dcb);
  dcb.BaudRate=BaudRate;
  dcb.fBinary=TRUE;
  dcb.fDtrControl=DTR_CONTROL_DISABLE;
  dcb.fRtsControl=RTS_CONTROL_DISABLE;
  dcb.Parity=NOPARITY;
  dcb.StopBits=ONESTOPBIT;
  dcb.ByteSize =8;

  SetCommState(si->hComm, &dcb);
  return true;
}

bool SportIdent::tcpAddPort(int port, DWORD zeroTime)
{
  CloseCom("TCP");

  SI_StationInfo *si = findStation("TCP");

  if (!si) {
    SI_Info[n_SI_Info].ComPort="TCP";
    SI_Info[n_SI_Info].ThreadHandle=0;
    si=&SI_Info[n_SI_Info];
    n_SI_Info++;
  }

  si->tcpPort=port;
  si->localZeroTime=zeroTime;
  si->hComm=0;
  return true;
}

bool SportIdent::OpenCom(const char *com)
{
  CloseCom(com);

  SI_StationInfo *si = findStation(com);

  if (!si) {
    SI_Info[n_SI_Info].ComPort=com;
    SI_Info[n_SI_Info].ThreadHandle=0;
    si=&SI_Info[n_SI_Info];
    n_SI_Info++;
  }

  si->StationNumber=0;
  si->Extended=0;
  si->HandShake=0;
  si->StationMode=0;

  string comfile=string("//./")+com;
  si->hComm = CreateFile( comfile.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            0,
            OPEN_EXISTING,
            0,//FILE_FLAG_OVERLAPPED,
            0);

  if (si->hComm == INVALID_HANDLE_VALUE) {
    si->hComm=0;
    return false;   // error opening port; abort
  }

  GetCommTimeouts(si->hComm, &si->TimeOuts);
  COMMTIMEOUTS MyTimeOuts=si->TimeOuts;
  MyTimeOuts.ReadIntervalTimeout=50;
  MyTimeOuts.ReadTotalTimeoutMultiplier = 10;
  MyTimeOuts.ReadTotalTimeoutConstant = 300;
  MyTimeOuts.WriteTotalTimeoutMultiplier = 10;
  MyTimeOuts.WriteTotalTimeoutConstant = 300;

  SetCommTimeouts(si->hComm, &MyTimeOuts);

  DCB dcb;
  memset(&dcb, 0, sizeof(dcb));
  dcb.DCBlength=sizeof(dcb);
  dcb.BaudRate=CBR_38400;
  dcb.fBinary=TRUE;
  dcb.fDtrControl=DTR_CONTROL_DISABLE;
  dcb.fRtsControl=RTS_CONTROL_DISABLE;
  dcb.Parity=NOPARITY;
  dcb.StopBits=ONESTOPBIT;
  dcb.ByteSize =8;

  SetCommState(si->hComm, &dcb);

  BYTE c[128];

  c[0]=WAKEUP;
  c[1]=STX;
  c[2]=STX;
  c[3]=0xF0;
  c[4]=0x01;
  c[5]=0x4D;
  SetCRC(c+3);

  c[8]=ETX;

  DWORD written;

  WriteFile(si->hComm, c, 9, &written, NULL);
  Sleep(700);
  //c[6]=
  DWORD read;
  BYTE buff[128];
  memset(buff, 0, sizeof(buff));
  read = ReadBytes(buff, 1, si->hComm);

  if (read == 1 && buff[0] == 0xFF){
    Sleep(100);
    read = ReadBytes(buff, 1, si->hComm);
  }

  if (read==1 && buff[0]==STX) {
    ReadFile(si->hComm, buff, 8, &read, NULL);

    ReadSystemData(si);
  }
  else {
    dcb.BaudRate=CBR_4800;
    SetCommState(si->hComm, &dcb);

    WriteFile(si->hComm, c, 9, &written, NULL);
    Sleep(600);
    //c[6]=
    DWORD read;
    BYTE buff[128];

    read=ReadByte(*buff, si->hComm);

    if (read==1 && buff[0]==STX) {
      ReadFile(si->hComm, buff, 8, &read, NULL);
      ReadSystemData(si);
    }
    else {
      BYTE cold[8];
      cold[0]=STX;
      cold[1]=0x70;
      cold[2]=0x4D;
      cold[3]=ETX;

      WriteFile(si->hComm, cold, 4, &written, NULL);

      Sleep(500);

      read=ReadByte(*buff, si->hComm);

      if (read!=1 || buff[0]!=STX) {
        CloseCom(si->ComPort.c_str());
        return false;
      }

      read=ReadBytesDLE_delay(buff, 4, si->hComm);
      if (read==4) {
        if (buff[1]>30)
          si->StationNumber=buff[1];
        else{
          si->StationNumber=buff[2];

        }
        if (buff[3]!=ETX)
          ReadByte(buff[4], si->hComm);
      }
    }
  }

  return true;
}


SI_StationInfo *SportIdent::findStation(const string &com)
{
  for(int i=0;i<n_SI_Info; i++)
    if (com == SI_Info[i].ComPort)
      return &SI_Info[i];

  return 0;
}

void SportIdent::CloseCom(const char *com)
{
  if (com==0)
  {
    for(int i=0;i<n_SI_Info; i++)
      CloseCom(SI_Info[i].ComPort.c_str());
  }
  else
  {
    SI_StationInfo *si = findStation(com);

    if (si && si->ComPort=="TCP") {
      if (tcpPortOpen) {
        EnterCriticalSection(&SyncObj);
        shutdown(SOCKET(serverSocket), SD_BOTH);
        LeaveCriticalSection(&SyncObj);

        Sleep(300);

        EnterCriticalSection(&SyncObj);
        closesocket(SOCKET(serverSocket));
        tcpPortOpen = 0;
        serverSocket = 0;
        LeaveCriticalSection(&SyncObj);
      }
    }
    else if (si && si->hComm)
    {
      if (si->ThreadHandle)
      {
        EnterCriticalSection(&SyncObj);
        TerminateThread(si->ThreadHandle, 0);
        LeaveCriticalSection(&SyncObj);

        CloseHandle(si->ThreadHandle);
        si->ThreadHandle=0;
      }
      SetCommTimeouts(si->hComm, &si->TimeOuts); //Restore
      CloseHandle(si->hComm);
      si->hComm=0;
    }
  }
}


int SportIdent::ReadByte_delay(BYTE &byte, HANDLE hComm)
{
  byte=0;

  return ReadBytes_delay(&byte, 1, hComm);
}


int SportIdent::ReadByte(BYTE &byte, HANDLE hComm)
{
  byte=0;

  if (!hComm)
    return -1;

  DWORD dwRead;

  if (ReadFile(hComm, &byte, 1, &dwRead, NULL))
  {
#ifdef DEBUG_SI2
    char t[64];
    sprintf_s(t, 64, "read=%02X\n", (int)byte);
    OutputDebugString(t);
#endif
    if (dwRead)
      return 1;
    else return 0;
  }
  else return -1;
}



int SportIdent::ReadBytes_delay(BYTE *byte, DWORD len,  HANDLE hComm)
{
  int read=0;
  int toread=len;
  int d;
  for(d=0;d<7 && read<toread;d++)
  {
    int r=ReadBytes(byte+read, len, hComm);

    if (r==-1) return -1;

    read+=r;

    if (read<toread)
    {
      len=toread-read;
      Sleep(100);
    }
  }
#ifdef DEBUG_SI2
  char t[64];
  sprintf_s(t, 64, "retry=%d\n", d);
  OutputDebugString(t);

  for (int k = 0; k < read; k++) {
    char t[64];
    sprintf_s(t, 64, "mreadd=%02X\n", (int)byte[k]);
    OutputDebugString(t);
  }
#endif

  return read;
}

int SportIdent::ReadBytes(BYTE *byte, DWORD len,  HANDLE hComm)
{
  if (!hComm)
    return -1;

  DWORD dwRead;

  if (ReadFile(hComm, byte, len, &dwRead, NULL))
  {
#ifdef DEBUG_SI2
    for (int k = 0; k < dwRead; k++) {
      char t[64];
      sprintf_s(t, 64, "mread=%02X\n", (int)byte[k]);
      OutputDebugString(t);
    }
#endif
    return dwRead;
  }
  else return -1;
}


int SportIdent::ReadBytesDLE_delay(BYTE *byte, DWORD len,  HANDLE hComm)
{
  int read=0;
  int toread=len;
  int d;

  for(d=0;d<15 && read<toread;d++)
  {
    int r=ReadBytes(byte+read, len, hComm);

    if (r==-1) return -1;

    read+=r;

    if (read<toread)
    {
      len=toread-read;
      Sleep(100);
    }
  }
#ifdef DEBUG_SI2
  char t[64];
  sprintf_s(t, 64, "retry=%d\n", d);
  OutputDebugString(t);
#endif

  return read;
}


int SportIdent::ReadBytesDLE(BYTE *byte, DWORD len,  HANDLE hComm)
{
  if (!hComm)
    return -1;

  DWORD dwRead;

  if (ReadFile(hComm, byte, len, &dwRead, NULL))
  {
    if (dwRead > 0)
    {
      DWORD ip=0;
      DWORD op=0;

      for (ip=0;ip<dwRead-1;ip++) {
        if (byte[ip]==DLE)
          byte[op++]=byte[++ip];
        else
          byte[op++]=byte[ip];
      }

      if (ip<dwRead) {
        if (byte[ip]==DLE)
          ReadByte(byte[op++], hComm);
        else
          byte[op++]=byte[ip];
      }

      if (op<len)
        return op+ReadBytesDLE(byte+op, len-op, hComm);
      else return len;
    }
    return 0;
  }
  else return -1;
}


struct SIOnlinePunch {
  BYTE Type;  //0=punch, 255=Triggered time
  WORD CodeNo;  //2 byte 0-65K
  DWORD SICardNo; //4 byte integer  -2GB until +2GB
  DWORD CodeDay; //Obsolete, not used anymore
  DWORD CodeTime;  //Time
};

int SportIdent::MonitorTCPSI(WORD port, int localZeroTime)
{
  tcpPortOpen=0;
  serverSocket=0;
  //A SOCKET is simply a typedef for an unsigned int.
  //In Unix, socket handles were just about same as file
  //handles which were again unsigned ints.
  //Since this cannot be entirely true under Windows
  //a new data type called SOCKET was defined.

  SOCKET server;

  //WSADATA is a struct that is filled up by the call
  //to WSAStartup
  WSADATA wsaData;

  //The sockaddr_in specifies the address of the socket
  //for TCP/IP sockets. Other protocols use similar structures.
  sockaddr_in local;

  //WSAStartup initializes the program for calling WinSock.
  //The first parameter specifies the highest version of the
  //WinSock specification, the program is allowed to use.
  int wsaret=WSAStartup(0x101,&wsaData);

  //WSAStartup returns zero on success.
  //If it fails we exit.
  if (wsaret!=0) {
      return 0;
  }

  //Now we populate the sockaddr_in structure
  local.sin_family=AF_INET; //Address family
  local.sin_addr.s_addr=INADDR_ANY; //Wild card IP address
  local.sin_port=htons((u_short)port); //port to use

  //the socket function creates our SOCKET
  server=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

  //If the socket() function fails we exit
  if (server==INVALID_SOCKET)
      return 0;

  //bind links the socket we just created with the sockaddr_in
  //structure. Basically it connects the socket with
  //the local address and a specified port.
  //If it returns non-zero quit, as this indicates error

  if (bind(server,(sockaddr*)&local,sizeof(local))!=0) {
    closesocket(server);
    return 0;
  }
  //listen instructs the socket to listen for incoming
  //connections from clients. The second arg is the backlog

  if (listen(server,10)!=0) {
    closesocket(server);
    return 0;
  }
  //we will need variables to hold the client socket.
  //thus we declare them here.

  SOCKET client;
  sockaddr_in from;
  int fromlen=sizeof(from);

  SIOnlinePunch op;

  tcpPortOpen=port;
  serverSocket=server;

  while (tcpPortOpen) {
    client=accept(server, (sockaddr*)&from, &fromlen);

    if (client != INVALID_SOCKET) {

      char temp[15];

      int r=0;
      while (r!=-1 && tcpPortOpen) {

        DWORD timeout = GetTickCount() + 1000;
        int iter = 0;
        while(r!=SOCKET_ERROR && r<15 && tcpPortOpen) {
          r=recv(client, temp, 15, MSG_PEEK);
          iter++;
          if (iter > 10) {
            if (GetTickCount() > timeout) {
              break;
            }
            else
              iter = 0;
          }
          Sleep(0);
        }

        r=recv(client, temp, 15, 0);

        if (r==15) {
          //BYTE c[15]={0, 0x64,0,0x48, 0xa4, 0x07, 00, 0x05, 00, 00, 00, 0x8d, 0x16, 0x06, 0x00};
          //memcpy(temp, c, 15);
          op.Type=temp[0];
          op.CodeNo=*(WORD*)(&temp[1]);
          op.SICardNo=*(DWORD *)(&temp[3]);
          op.CodeDay=*(DWORD *)(&temp[7]);
          op.CodeTime=*(DWORD *)(&temp[11]);

          if (op.Type == 64 &&  op.CodeNo>1 &&  op.CodeNo<=192 && op.CodeTime == 0) {
            // Recieved card
            int nPunch = op.CodeNo;
            vector<SIPunch> punches(nPunch);
            r = recv(client, (char*)&punches[0], 8 * nPunch, 0);
            if (r == 8 * nPunch) {
              SICard card;
              card.CheckPunch.Code = -1;
              card.StartPunch.Code = -1;
              card.FinishPunch.Code = -1;
              card.CardNumber = op.SICardNo;
              for (int k = 0; k < nPunch; k++) {
                punches[k].Time /= 10;
                if (punches[k].Code == oPunch::PunchStart)
                  card.StartPunch = punches[k];
                else if (punches[k].Code == oPunch::PunchFinish)
                  card.FinishPunch = punches[k];
                else if (punches[k].Code == oPunch::PunchCheck)
                  card.CheckPunch = punches[k];
                else
                  card.Punch[card.nPunch++] = punches[k];
              }
              addCard(card);
            }
          }
          else
            AddPunch(op.CodeTime/10, op.CodeNo, op.SICardNo, 0);
        }
        else r=-1;

      }
      //close the client socket
      closesocket(client);
    }
    else {
      int err=WSAGetLastError();

      if (err==WSAESHUTDOWN || err==WSAECONNABORTED) {
        tcpPortOpen=0;
        serverSocket=0;
        closesocket(server);
        //MessageBox(0, "TCP CLOSE", 0, MB_OK);
        return 0;
      }
    }
  }

  //MessageBox(0, "TCP CLOSE BYE", 0, MB_OK);

  //closesocket() closes the socket and releases the socket descriptor
  closesocket(server);

  WSACleanup();
  return 1;
}

bool SportIdent::MonitorSI(SI_StationInfo &si)
{
  HANDLE hComm=si.hComm;

  if (!hComm)
    return false;

  DWORD dwCommEvent;
  DWORD dwRead;
  BYTE  chRead;

  if (!SetCommMask(hComm, EV_RXCHAR))
  {
     // Error setting communications event mask.
    //Sleep(1000);
    return false;
  }
  else
  {
  for ( ; ; ) {
     if (WaitCommEvent(hComm, &dwCommEvent, NULL)) {
      if (dwCommEvent==EV_RXCHAR)  do{
       if ( (dwRead=ReadByte(chRead,  hComm))!=-1)
       {
        // A byte has been read; process it.
         if (chRead==STX && ReadByte(chRead,  hComm))
         {
          switch(chRead)
          {
          case 0xD3:{//Control Punch, extended mode.
            BYTE bf[32];
            bf[0]=chRead;
            ReadBytes(bf+1, 17,  hComm);
            if (CheckCRC(LPBYTE(bf)))
            {
              WORD Station=MAKEWORD(bf[3], bf[2]);

              DWORD ShortCard=MAKEWORD(bf[7], bf[6]);
              DWORD Series=bf[5];


              DWORD Card=MAKELONG(MAKEWORD(bf[7], bf[6]), MAKEWORD(bf[5], bf[4]));

              if (Series<=4 && Series>=1)
                Card=ShortCard+100000*Series;

              DWORD Time=0;
              if (bf[8]&0x1) Time=3600*12;
              Time+=MAKEWORD(bf[10], bf[9]);
#ifdef DEBUG_SI
              char str[128];
              sprintf_s(str, "EXTENDED: Card = %d, Station = %d, StationMode = %d", Card, Station, si.StationMode);
              MessageBox(NULL, str, NULL, MB_OK);
#endif
              AddPunch(Time, Station & 1023, Card & 0x00FFFFFF, si.StationMode);
            }
            break;
          }
          case 0x53:{ //Control Punch, old mode
            BYTE bf[32];
            bf[0]=chRead;
            //Sleep(200);
            ReadBytesDLE(bf+1, 30, hComm);

            BYTE Station=bf[2];
            BYTE Series=bf[3];
            WORD Card=MAKEWORD(bf[5], bf[4]);


            DWORD DCard;

            if (Series==1)
              DCard=Card;
            else if (Series<=4)
              DCard=Card+100000*Series;
            else
              DCard=MAKELONG(Card, Series);

            //if (Series!=1)
            //	Card+=100000*Series;

            DWORD Time=MAKEWORD(bf[8], bf[7]);
            BYTE p=bf[1];
            if (p&0x1) Time+=3600*12;

#ifdef DEBUG_SI
              char str[128];
              sprintf_s(str, "OLD: Card = %d, Station = %d, StationMode = %d", DCard, Station, si.StationMode);
              MessageBox(NULL, str, NULL, MB_OK);
#endif
            AddPunch(Time, Station, DCard, si.StationMode);
            break;
          }
          case 0xE7:{//SI5/6 removed
            BYTE bf[32];
            ReadBytes(bf+1, 10,  hComm);
            break;
          }

          case 0xE6:{
            BYTE bf[32];
            bf[0]=0xE6;
            ReadBytes(bf+1, 10,  hComm);

            //ReadByte(chRead); //ETX!
            if (CheckCRC(LPBYTE(bf)))
              GetSI6DataExt(hComm);

            break;
          }
          case 0xE5:{
            BYTE bf[32];
            bf[0]=0xE5;
            ReadBytes(bf+1, 10,  hComm);

            if (CheckCRC(LPBYTE(bf)))
              GetSI5DataExt(hComm);

            break;
          }
          case 0x46:
            ReadByte(chRead,  hComm); //0x49?!
            ReadByte(chRead,  hComm); //ETX!
            GetSI5Data(hComm);
            break;
          case 0x66:{ //STX, 66h, CSI, TI, TP, CN3, CN2, CN1, CN0, ETX
            BYTE bf[32];
            //SICard5Detect si5;
            //si5.code=0xE5;
            bf[0]=0xE6;
            ReadBytesDLE(bf+1, 8,  hComm);

            GetSI6Data(hComm);

            break;
          }
          case 0xB1:{
            BYTE bf[200];
            bf[0]=chRead;
            ReadBytes(bf+1, 200,  hComm);
            //GetSI5DataExt(hComm);
            MessageBox(NULL, "Programmera stationen utan AUTOSEND!", NULL, MB_OK);
            }
            break;

          case 0xE1:{
            BYTE bf[200];
            bf[0]=chRead;
            ReadBytes(bf+1, 200,  hComm);
            MessageBox(NULL, "Programmera stationen utan AUTOSEND!", NULL, MB_OK);
            }
            break;
          case 0xE8:{
            BYTE bf[32];
            bf[0]=0xE8;
            ReadBytes(bf+1, 10,  hComm);

            //ReadByte(chRead); //ETX!
            if (CheckCRC(LPBYTE(bf)))
              GetSI9DataExt(hComm);

            break;
          }
          //	MessageBox(NULL, "SI-card not supported", NULL, MB_OK);
          //	break;
          default:

            BYTE bf[128];
            bf[0]=chRead;
            int rb=ReadBytes(bf+1, 120,  hComm);

            string st;
            for(int k=0;k<=rb;k++){
              if (bf[k]==STX)
                st+="STX ";
              else if (bf[k]==ETX)
                st+="ETX ";
              else if (bf[k]==ACK)
                st+="ACK ";
              else if (bf[k]==DLE)
                st+="DLE ";
              else{
                char d[10];
                sprintf_s(d, "%02X (%d) ", bf[k], bf[k]);
                st+=d;
              }
            }
            //MessageBox(NULL, st.c_str(), "Unknown SI response", MB_OK);
          }
         }
       }
       else
       {
        // An error occurred in the ReadFile call.
        return false;
       }
      } while (dwRead);
      else
      {
      //   MessageBox(hWndNotify, "EXIT 1", NULL, MB_OK);
       // Error in WaitCommEvent
       //  return false;
      }
     }
     else
     {
        for(int i=0;i<n_SI_Info; i++)
        if (hComm==SI_Info[i].hComm)
        {
          //We are about to kill this thread the natural way
          //Prevent forced kill

          //CloseHandle(SI_Info[i].ThreadHandle);
          SI_Info[i].ThreadHandle=0;

          CloseCom(SI_Info[i].ComPort.c_str());
        }

      //Notify something happened...
      PostMessage(hWndNotify, WM_USER+1, ClassId, 0);
      // MessageBox(hWndNotify, "EXIT 2", NULL, MB_OK);
      // Error in WaitCommEvent
      return false;
     }
  }
  }
  MessageBox(hWndNotify, "EXIT 3", NULL, MB_OK);
  return true;
}


void SportIdent::GetSI5DataExt(HANDLE hComm)
{
  BYTE c[128];

  c[0]=STX;
  c[1]=0xB1;
  c[2]=0x00;
  SetCRC(c+1);
  c[5]=ETX;

  DWORD written=0;
  WriteFile(hComm, c, 5, &written, NULL);

  if (written==5)
  {
    Sleep(150);
    BYTE bf[256];
    memset(bf, 0, 256);

    ReadBytes(bf, 128+8, hComm);

    if (bf[0]==STX && bf[1]==0xB1)
    {
      if (CheckCRC(bf+1))
      {
        c[0]=ACK;
        WriteFile(hComm, c, 1, &written, NULL);

        BYTE *Card5Data=bf+5;
        SICard card;
        GetCard5Data(Card5Data, card);
        addCard(card);
      }
    }
    //Sleep(1000);
  }
}


void SportIdent::GetSI6DataExt(HANDLE hComm)
{
  BYTE b[128*8];
  memset(b, 0, 128*8);
  BYTE c[16];
//	STX, 0xE1, 0x01, BN, CRC1,
//CRC0, ETX
  OutputDebugString("STARTREAD EXT-");

  int blocks[7]={0,6,7,2,3,4,5};
  DWORD written=0;

  for(int k=0;k<7;k++)
  {
    c[0]=STX;
    c[1]=0xE1;
    c[2]=0x01;
    c[3]=blocks[k];
    SetCRC(c+1);
    c[6]=ETX;

    written=0;
    WriteFile(hComm, c, 7, &written, NULL);

    if (written==7) {
      Sleep(50);
      BYTE bf[256];
      memset(bf, 0, 256);

      int read=ReadBytes(bf, 128+9, hComm);

      if (read==0) {
        OutputDebugString("TIMING");
        Sleep(300);
        read = ReadBytes(bf, 128+9, hComm);
      }

      if (bf[0]==STX && bf[1]==0xE1) {
        if (CheckCRC(bf+1)) {
          memcpy(b+k*128, bf+6, 128);

          LPDWORD ptr = LPDWORD(bf + 6);
          if (ptr[31]==0xEEEEEEEE)
            break; //No need to read more
        }
        else {
          OutputDebugString("-FAIL-");
          return;
        }
      }
      else {
        OutputDebugString("-FAIL-");
        return;
      }
    }
  }

  c[0]=ACK;
  WriteFile(hComm, c, 1, &written, NULL);

  OutputDebugString("-ACK-");

  SICard card;
  GetCard6Data(b, card);
  addCard(card);
}

void SportIdent::GetSI9DataExt(HANDLE hComm)
{
  BYTE b[128*5];
  memset(b, 0, 128*5);
  BYTE c[16];
//	STX, 0xE1, 0x01, BN, CRC1,
//CRC0, ETX
  OutputDebugString("STARTREAD9 EXT-");

  int blocks_8_9_p_t[2]={0,1};
  int blocks_10_11_SIAC[5]={0,4,5,6,7};
  int limit = 1;
  int *blocks = blocks_8_9_p_t;

  DWORD written=0;

  for(int k=0; k < limit; k++){
    c[0]=STX;
    c[1]=0xEF;
    c[2]=0x01;
    c[3]=blocks[k];
    SetCRC(c+1);
    c[6]=ETX;

    written=0;
    WriteFile(hComm, c, 7, &written, NULL);

    if (written==7) {
      Sleep(50);
      BYTE bf[256];
      memset(bf, 0, 256);

      int read=ReadBytes(bf, 128+9, hComm);

      if (read==0) {
        OutputDebugString("TIMING");
        Sleep(300);
        read = ReadBytes(bf, 128+9, hComm);
      }

      if (bf[0]==STX && bf[1]==0xEf) {
        if (CheckCRC(bf+1)) {
          memcpy(b+k*128, bf+6, 128);

        if (k == 0) {
          int series = b[24] & 15;
          if (series == 15) {
            int nPunch = min(int(b[22]), 128);
            blocks = blocks_10_11_SIAC;
            limit = 1 + (nPunch+31) / 32;
          }
          else {
            limit = 2; // Card 8, 9, p, t
          }
        }

        }
        else {
          OutputDebugString("-FAIL-");
          return;
        }
      }
      else {
        OutputDebugString("-FAIL-");
        return;
      }
    }
  }

  c[0]=ACK;
  WriteFile(hComm, c, 1, &written, NULL);

  OutputDebugString("-ACK-");

  SICard card;
  if (GetCard9Data(b, card))
    addCard(card);
}

bool SportIdent::ReadSI6Block(HANDLE hComm, BYTE *data)
{
  BYTE bf[256];
  memset(bf, 0, 256);

  int read=ReadBytesDLE(bf, 4, hComm);

  if (read==0){
    OutputDebugString("TIMING");
    Sleep(1000);
    read=ReadBytesDLE(bf, 4, hComm);
  }

  if (bf[0]==STX && bf[1]==0x61)
  {
    BYTE sum=bf[2]+bf[3]; //Start calc checksum

    read=ReadBytesDLE(bf, 128, hComm);
    BYTE cs, etx;
    ReadBytesDLE(&cs, 1, hComm);
    ReadByte(etx, hComm);

    for(int i=0;i<128;i++)
      sum+=bf[i];

    if (sum==cs)
    {
      memcpy(data, bf, 128);
      return true;
    }
    else return false;
  }

  return false;
}


void SportIdent::GetSI6Data(HANDLE hComm)
{
  BYTE b[128*8];
  memset(b, 0, 128*8);
  BYTE c[16];
//	STX, 0xE1, 0x01, BN, CRC1,
//CRC0, ETX
  OutputDebugString("STARTREAD-");

  //int blocks[3]={0,6,7};
  DWORD written=0;


  c[0]=STX;
  c[1]=0x61;
  c[2]=0x08;
  c[3]=ETX;

  written=0;
  WriteFile(hComm, c, 4, &written, NULL);
  bool compact = false;
  if (written==4) {
    Sleep(500);

    for(int k=0;k<8;k++){

      if (!ReadSI6Block(hComm, b+k*128)) {
        if (k<=2) {
          OutputDebugString("-FAIL-");
          return;
        }
        else
          break;
      }
      if (k>2)
        compact = true;
    }
  }

  if (compact) {
    BYTE b2[128*7];
    // 192 punches, sicard6 star
    for (int k = 0; k<128*8; k++) {
      if (k<128)
        b2[k] = b[k];
      else if (k>=256 && k<128*6)
        b2[k+128] = b[k];
      else if (k>=128*6)
        b2[k-128*5] = b[k];
    }
    memcpy(b, b2, sizeof(b2));
  }

  c[0]=ACK;
  WriteFile(hComm, c, 1, &written, NULL);

  OutputDebugString("-ACK-");
  SICard card;
  GetCard6Data(b, card);

  addCard(card);
}

void SportIdent::GetSI5Data(HANDLE hComm)
{
  BYTE c[128];

  c[0]=STX;
  c[1]=0x31;
  c[2]=ETX;

  DWORD written=0;
  WriteFile(hComm, c, 3, &written, NULL);

  if (written==3)
  {
    Sleep(900);
    BYTE bf[256];
    memset(bf, 0, 256);

    int read=ReadBytesDLE(bf, 3, hComm);

    if (read==0)
    {
      Sleep(1000);
      read=ReadBytesDLE(bf, 3, hComm);
    }

    if (bf[0]==STX && bf[1]==0x31)
    {

      BYTE sum=bf[2]; //Start calc checksum

      read=ReadBytesDLE(bf, 128, hComm);
      BYTE cs, etx;
      ReadBytesDLE(&cs, 1, hComm);
      ReadByte(etx, hComm);

      for(int i=0;i<128;i++)
        sum+=bf[i];

      if (sum==cs)
      {
        c[0]=ACK;
        WriteFile(hComm, c, 1, &written, NULL);

        //BYTE *Card5Data=bf+5;
        SICard card;
        GetCard5Data(bf, card);

        addCard(card);
      }
    }
  }
}



bool SportIdent::GetCard5Data(BYTE *data, SICard &card)
{
/*	ofstream fout("si.txt");

  for(int m=0;m<128;m++)
  {
    if (m%16==0) fout << endl;

    char bf[16];
    sprintf(bf, "%02x ", (DWORD)data[m]);
    fout << bf;
  }

  fout << endl;
  return 0;
*/
  memset(&card, 0, sizeof(card));

  DWORD number=MAKEWORD(data[5], data[4]);

  if (data[6]==1)
    card.CardNumber=number;
  else
    card.CardNumber=100000*data[6]+number;

  data+=16;


  AnalyseSI5Time(data+3, card.StartPunch.Time, card.StartPunch.Code);
  AnalyseSI5Time(data+5, card.FinishPunch.Time, card.FinishPunch.Code);
  AnalyseSI5Time(data+9, card.CheckPunch.Time, card.CheckPunch.Code);

//	card.StartPunch=MAKEWORD(data[4], data[3]);
//	card.FinishPunch=MAKEWORD(data[6], data[5]);
  card.nPunch=data[7]-1;

  data+=16;

  for (DWORD k=0;k<card.nPunch;k++) {
    if (k<30) {
      DWORD basepointer=3*(k%5)+1+(k/5)*16;
      DWORD code=data[basepointer];
      DWORD time;//=MAKEWORD(data[basepointer+2],data[basepointer+1]);
      DWORD slask;
      AnalyseSI5Time(data+basepointer+1, time, slask);

      card.Punch[k].Code=code;
      card.Punch[k].Time=time;
    }
    else if (k<36) {
      card.Punch[k].Code=data[ (k-30)*16 ];
      card.Punch[k].Time=0; //No time for extra punch
    }
  }

  return true;
}

DWORD SportIdent::GetExtCardNumber(BYTE *data) const {
  DWORD cnr = 0;
  BYTE *p = (BYTE *)&cnr;
  p[0] = data[27];
  p[1] = data[26];
  p[2] = data[25];

  return cnr;
}

bool SportIdent::GetCard9Data(BYTE *data, SICard &card)
{
  /*ofstream fout("si.txt");

  for(int m=0;m<128*2;m++)
  {
    if (m%16==0) fout << endl;

    char bf[16];
    sprintf_s(bf, 16, "%02x ", (DWORD)data[m]);
    fout << bf;
  }

  fout << endl;*/
  //return 0;

  memset(&card, 0, sizeof(card));


  card.CardNumber = GetExtCardNumber(data);

  int series = data[24] & 15;

  AnalysePunch(data+12, card.StartPunch.Time, card.StartPunch.Code);
  AnalysePunch(data+16, card.FinishPunch.Time, card.FinishPunch.Code);
  AnalysePunch(data+8, card.CheckPunch.Time, card.CheckPunch.Code);

  if (series == 1) {
    // SI Card 9
    card.nPunch=min(int(data[22]), 50);
    for(unsigned k=0;k<card.nPunch;k++) {
      AnalysePunch(14*4 + data + 4*k, card.Punch[k].Time, card.Punch[k].Code);
    }
  }
  else if (series == 2) {
    // SI Card 8
    card.nPunch=min(int(data[22]), 30);
    for(unsigned k=0;k<card.nPunch;k++) {
      AnalysePunch(34*4 + data + 4*k, card.Punch[k].Time, card.Punch[k].Code);
    }
  }
  else if (series == 4) {
    // pCard
    card.nPunch=min(int(data[22]), 20);
    for(unsigned k=0;k<card.nPunch;k++) {
      AnalysePunch(44*4 + data + 4*k, card.Punch[k].Time, card.Punch[k].Code);
    }
  }
  else if (series == 15) {
    // Card 10, 11, SIAC
    card.nPunch=min(int(data[22]), 128);
    for(unsigned k=0;k<card.nPunch;k++) {
      AnalysePunch(data + 128 + 4*k, card.Punch[k].Time, card.Punch[k].Code);
    }
  }
  else
    return false;

  return true;
}

bool SportIdent::GetCard6Data(BYTE *data, SICard &card)
{
  /*ofstream fout("si.txt");

  for(int m=0;m<128*3;m++)
  {
    if (m%16==0) fout << endl;

    char bf[16];
    sprintf_s(bf, 16, "%02x ", (DWORD)data[m]);
    fout << bf;
  }

  fout << endl;*/
  //return 0;

  memset(&card, 0, sizeof(card));

  WORD hi=MAKEWORD(data[11], data[10]);
  WORD lo=MAKEWORD(data[13], data[12]);

  card.CardNumber=MAKELONG(lo, hi);

  data+=16;

//	DWORD control;
//	DWORD time;

  AnalysePunch(data+8, card.StartPunch.Time, card.StartPunch.Code);
  AnalysePunch(data+4, card.FinishPunch.Time, card.FinishPunch.Code);
  AnalysePunch(data+12, card.CheckPunch.Time, card.CheckPunch.Code);
  card.nPunch=min(int(data[2]), 192);

  int i;

  memcpy(card.LastName, data+32, 20);
  for(i=19;i>0 && card.LastName[i]==0x20;i--)
    card.LastName[i]=0;

  memcpy(card.FirstName, data+32+20, 20);
  for(i=19;i>0 && card.FirstName[i]==0x20;i--)
    card.FirstName[i]=0;

  data+=128-16;

  for(unsigned k=0;k<card.nPunch;k++)
  {
    AnalysePunch(data+4*k, card.Punch[k].Time, card.Punch[k].Code);
  }

  // Check for extra punches, SI6-bug
  for (unsigned k = card.nPunch; k < 192; k++) {
    if (!AnalysePunch(data+4*k, card.Punch[k].Time, card.Punch[k].Code)) {
      break;
    }
    else {
      card.nPunch++; // Extra punch
      card.Punch[k].Code += 1000; // Mark as special
    }
  }

  return true;
}

bool SportIdent::AnalysePunch(BYTE *data, DWORD &time, DWORD &control) {
  if (*LPDWORD(data)!=0xEEEEEEEE && *LPDWORD(data)!=0x0)
  {
    BYTE ptd=data[0];
    BYTE cn=data[1];
    BYTE pth=data[2];
    BYTE ptl=data[3];

    control=cn+256*((ptd>>6)&0x3);
    time=MAKEWORD(ptl, pth)+3600*12*(ptd&0x1);
    return true;
  }
  else
  {
    control=-1;
    time=0;
    return false;
  }
}

void SportIdent::AnalyseSI5Time(BYTE *data, DWORD &time, DWORD &control)
{
  if (*LPWORD(data)!=0xEEEE) {
    time=MAKEWORD(data[1], data[0]);

    if (ZeroTime<12*3600) {
      //F�rmiddag
      if ( time < ZeroTime )
        time+=12*3600; //->Eftermiddag
    }
    else {
      //Eftermiddag

      if ( time >= ZeroTime%(12*3600) ) {
        //Eftermiddag
        time+=12*3600;
      }
      // else Efter midnatt OK.
    }
    control=0;
  }
  else {
    control=-1;
    time=0;
  }
}


void SportIdent::EnumrateSerialPorts(list<int> &ports)
{
  //Make sure we clear out any elements which may already be in the array
  ports.clear();

  //Determine what OS we are running on
  OSVERSIONINFO osvi;
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  BOOL bGetVer = GetVersionEx(&osvi);

  //On NT use the QueryDosDevice API
  if (bGetVer && (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT))
  {
    //Use QueryDosDevice to look for all devices of the form COMx. This is a better
    //solution as it means that no ports have to be opened at all.
    TCHAR szDevices[65535];
    DWORD dwChars = QueryDosDevice(NULL, szDevices, 65535);
    if (dwChars)
    {
      int i=0;

      for (;;)
      {
        //Get the current device name
        TCHAR* pszCurrentDevice = &szDevices[i];

        //If it looks like "COMX" then
        //add it to the array which will be returned
        int nLen = _tcslen(pszCurrentDevice);
        if (nLen > 3 && _tcsnicmp(pszCurrentDevice, _T("COM"), 3) == 0)
        {
          //Work out the port number
          int nPort = _ttoi(&pszCurrentDevice[3]);
          ports.push_front(nPort);
        }

        // Go to next NULL character
        while(szDevices[i] != _T('\0'))
          i++;

        // Bump pointer to the next string
        i++;

        // The list is double-NULL terminated, so if the character is
        // now NULL, we're at the end
        if (szDevices[i] == _T('\0'))
          break;
      }
    }
    //else
    //  TRACE(_T("Failed in call to QueryDosDevice, GetLastError:%d\n"), GetLastError());
  }
  else
  {
    //On 95/98 open up each port to determine their existence

    //Up to 255 COM ports are supported so we iterate through all of them seeing
    //if we can open them or if we fail to open them, get an access denied or general error error.
    //Both of these cases indicate that there is a COM port at that number.
    for (UINT i=1; i<256; i++)
    {
      //Form the Raw device name
      char sPort[256];

      sprintf_s(sPort, 256, "\\\\.\\COM%d", i);

      //Try to open the port
      BOOL bSuccess = FALSE;
      HANDLE hPort = ::CreateFile(sPort, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
      if (hPort == INVALID_HANDLE_VALUE)
      {
        DWORD dwError = GetLastError();

        //Check to see if the error was because some other app had the port open or a general failure
        if (dwError == ERROR_ACCESS_DENIED || dwError == ERROR_GEN_FAILURE)
          bSuccess = TRUE;
      }
      else
      {
        //The port was opened successfully
        bSuccess = TRUE;

        //Don't forget to close the port, since we are going to do nothing with it anyway
        CloseHandle(hPort);
      }

      //Add the port number to the array which will be returned
      if (bSuccess)
        ports.push_front(i);
    }
  }
}



void SportIdent::addCard(const SICard &sic)
{
  EnterCriticalSection(&SyncObj);
  try {
    ReadCards.push_front(sic);
  }
  catch(...) {
    LeaveCriticalSection(&SyncObj);
    throw;
  }
  LeaveCriticalSection(&SyncObj);

  PostMessage(hWndNotify, WM_USER, ClassId, 0);
}

void SportIdent::AddPunch(DWORD Time, int Station, int Card, int Mode)
{
  SICard sic;
  memset(&sic, 0, sizeof(sic));
  sic.CardNumber=Card;
  sic.StartPunch.Code = -1;
  sic.CheckPunch.Code = -1;
  sic.FinishPunch.Code = -1;

  if (Mode==0){
    if (Station>30){
      sic.Punch[0].Code=Station;
      sic.Punch[0].Time=Time;
      sic.nPunch=1;
    }
    else if (Station == oPunch::PunchStart) {
      sic.StartPunch.Time = Time;
      sic.StartPunch.Code = oPunch::PunchStart;
    }
    else if (Station == oPunch::PunchCheck) {
      sic.CheckPunch.Time = Time;
      sic.CheckPunch.Code = oPunch::PunchCheck;
    }
    else{
      sic.FinishPunch.Time=Time;
      sic.FinishPunch.Code = oPunch::PunchFinish;
    }
  }
  else{
    if (Mode==0x02 || Mode == 50){
      sic.Punch[0].Code=Station;
      sic.Punch[0].Time=Time;
      sic.nPunch=1;
    }
    else if (Mode == 3) {
      sic.StartPunch.Time=Time;
      sic.StartPunch.Code = oPunch::PunchStart;
    }
    else if (Mode == 10) {
      sic.CheckPunch.Time=Time;
      sic.CheckPunch.Code = oPunch::PunchCheck;
    }
    else{
      sic.FinishPunch.Time=Time;
      sic.FinishPunch.Code = oPunch::PunchFinish;
    }
  }
  sic.PunchOnly=true;

  addCard(sic);
}


bool SportIdent::GetCard(SICard &sic)
{
  bool ret=false;

  EnterCriticalSection(&SyncObj);

  if (!ReadCards.empty()) {
    sic=ReadCards.front();
    ReadCards.pop_front();
    ret=true;
  }

  LeaveCriticalSection(&SyncObj);

  return ret;
}

void start_si_thread(void *ptr)
{
  SportIdent *si=(SportIdent *)ptr;

  if (!si->Current_SI_Info)
    return;

  EnterCriticalSection(&si->SyncObj);
  SI_StationInfo si_info=*si->Current_SI_Info;	//Copy data.
  si->Current_SI_Info=0;
  LeaveCriticalSection(&si->SyncObj);

  if (si_info.ComPort=="TCP") {
    si->MonitorTCPSI(si_info.tcpPort, si_info.localZeroTime);
  }
  else {
    if (!si_info.hComm)  MessageBox(NULL, "ERROR", 0, MB_OK);
    si->MonitorSI(si_info);
  }
}

void SportIdent::StartMonitorThread(const char *com)
{
  SI_StationInfo *si = findStation(com);

  if (si && (si->hComm || si->ComPort=="TCP"))
  {
    if (si->ComPort=="TCP")
      tcpPortOpen=0;

    Current_SI_Info=si;
    si->ThreadHandle=(HANDLE)_beginthread(start_si_thread, 0,  this);

    while((volatile void *)Current_SI_Info)
      Sleep(0);

    if (si->ComPort=="TCP") {
      DWORD ec=0;
      while( (GetExitCodeThread(si->ThreadHandle, &ec)!=0 && ec==STILL_ACTIVE && tcpPortOpen==0))
        Sleep(0);
    }
  }
  else MessageBox(NULL, "ERROR", 0, MB_OK);
}

void checkport_si_thread(void *ptr)
{
  int *port=(int *)ptr;
  char bf[16];
  sprintf_s(bf, 16, "COM%d", *port);
  SportIdent si(NULL, *port);

  if (!si.OpenCom(bf))
    *port=0; //No SI found here
  else {
    bool valid = true;
    SI_StationInfo *sii = si.findStation(bf);
    if (sii) {
      if (sii->StationNumber>=1024 || sii->StationMode>10 ||
              !(sii->AutoSend || sii->HandShake))
        valid = false;
    }
    if (valid)
      *port = -(*port);
    else
      *port = 0;
  }
  si.CloseCom(0);
  //_endthread();
}


bool SportIdent::AutoDetect(list<int> &ComPorts)
{
  list<int> Ports;

  EnumrateSerialPorts(Ports);

  int array[128];
  //memset(array, 0, 128*sizeof(int));
  int i=0;

  while(!Ports.empty() && i<128)
  {
    array[i]=Ports.front();
    Ports.pop_front();
    (HANDLE)_beginthread(checkport_si_thread, 0,  &array[i]);
    i++;
    //Sleep(0);
  }

  int maxel=1;
  while(maxel>0)
  {
    Sleep(300);
    maxel=0;

    for(int k=0;k<i;k++)
      maxel=max(maxel, array[k]);
  }

  ComPorts.clear();

  for(int k=0;k<i;k++)
    if (array[k]<0)
      ComPorts.push_back(-array[k]);


// MessageBox(hWndNotify, "Detection complete", "OK", MB_OK);

  return !ComPorts.empty();
}

void SportIdent::SetZeroTime(DWORD zt)
{
  EnterCriticalSection(&SyncObj);
  ZeroTime=zt%(24*3600);
  LeaveCriticalSection(&SyncObj);

}

bool SportIdent::IsPortOpen(const string &com)
{
  SI_StationInfo *si = findStation(com);

  if (si && si->ComPort=="TCP")
    return tcpPortOpen && serverSocket;
  else
    return si!=0 && si->hComm && si->ThreadHandle;
}

string SportIdent::getInfoString(const string &com)
{
  SI_StationInfo *si = findStation(com);

  if (com=="TCP") {
    if (!si || !tcpPortOpen || !serverSocket)
      return "TCP: "+lang.tl("ej aktiv.");

    char bf[128];
    sprintf_s(bf, lang.tl("TCP: Port %d, Nolltid: %s").c_str(), tcpPortOpen, "00:00:00");
    return bf;
  }

  if (!(si!=0 && si->hComm && si->ThreadHandle))
    return com+": "+lang.tl("ej aktiv.");

  string info=si->ComPort;
  if (si->Extended) info+=lang.tl(": Ut�kat protokoll. ");
  else info+=lang.tl(": �ldre protokoll. ");

  switch(si->StationMode){
    case 2:
    case 50:
      info+=lang.tl("Kontrol");
      break;
    case 4:
      info+=lang.tl("M�l");
      break;
    case 3:
      info+=lang.tl("Start");
      break;
    case 5:
      info+=lang.tl("L�s brickor");
      break;
    case 7:
      info+=lang.tl("T�m");
      break;
    case 10:
      info+=lang.tl("Check");
      break;
    default:
      info+=lang.tl("Ok�nd funktion");
  }

  if (si->StationNumber) {
    char bf[16];
    sprintf_s(bf, " (%d).", si->StationNumber);
    info+=bf;
  }

  info += lang.tl(" Kommunikation: ");
  if (si->AutoSend) info+=lang.tl("skicka st�mplar.");
  else if (si->HandShake) info+=lang.tl("handskakning.");
  else info+=lang.tl("[VARNING] ingen/ok�nd.");

  return info;
}

vector<string> SICard::codeLogData(int row) const
{
  vector<string> log;

  log.push_back(itos(row));
  if (readOutTime[0] == 0)
    log.push_back(getLocalTime());
  else
    log.push_back(readOutTime);
  log.push_back(itos(CardNumber));
  log.push_back("");
  log.push_back("");
  log.push_back(FirstName);
  log.push_back(LastName);
  log.push_back(Club);
  log.push_back("");
  log.push_back("");
  log.push_back("");
  log.push_back(""); //email
  log.push_back("");
  log.push_back("");
  log.push_back("");
  log.push_back(""); //zip

  log.push_back(""); //CLR_NO
  log.push_back("");
  log.push_back("");

  //We set monday on every punch, since this is our way of
  //saying that we have 24-h clock.
  if (signed(CheckPunch.Code) >= 0) {
    log.push_back(itos(CheckPunch.Code));
    log.push_back("MO");
    log.push_back(formatTime(CheckPunch.Time));
  }
  else {
    log.push_back(""); //CHCK_NO
    log.push_back("");
    log.push_back("");
  }

  if (signed(StartPunch.Code) >= 0) {
    log.push_back(itos(StartPunch.Code));
    log.push_back("MO");
    log.push_back(formatTime(StartPunch.Time));
  }
  else {
    log.push_back(""); //START_NO
    log.push_back("");
    log.push_back("");
  }

  if (signed(FinishPunch.Code) >= 0) {
    log.push_back(itos(FinishPunch.Code));
    log.push_back("MO");
    log.push_back(formatTime(FinishPunch.Time));
  }
  else {
    log.push_back(""); //FINISH_NO
    log.push_back("");
    log.push_back("");
  }
  log.push_back(itos(nPunch));

  for (int k=0;k<192;k++) {
    if (k<int(nPunch)) {
      log.push_back(itos(Punch[k].Code));
      if (Punch[k].Time>0) {
        log.push_back("MO");
        log.push_back(formatTime(Punch[k].Time));
      }
      else {
        log.push_back("");
        log.push_back("");
      }
    }
    else {
      log.push_back("");
      log.push_back("");
      log.push_back("");
    }
  }
  return log;
}

vector<string> SICard::logHeader()
{
  vector<string> log;

  log.push_back("No.");
  log.push_back("read at");
  log.push_back("SI-Card");
  log.push_back("St no");
  log.push_back("cat");
  log.push_back("First name");
  log.push_back("name");
  log.push_back("club");
  log.push_back("country");
  log.push_back("sex");
  log.push_back("year-op");
  log.push_back("EMail");
  log.push_back("mobile");
  log.push_back("city");
  log.push_back("street");
  log.push_back("zip");

  log.push_back("CLR_CN");
  log.push_back("CLR_DOW");
  log.push_back("clear time");

  log.push_back("CHK_CN");
  log.push_back("CHK_DOW");
  log.push_back("check time");

  log.push_back("ST_CN");
  log.push_back("ST_DOW");
  log.push_back("start time");

  log.push_back("FI_CN");
  log.push_back("FO_DOW");
  log.push_back("Finish time");

  log.push_back("No. of punches");

  for (int k=0;k<192;k++) {
    string pf = itos(k+1);
    log.push_back(pf+".CN");
    log.push_back(pf+".DOW");
    log.push_back(pf+".Time");
  }
  return log;
}

unsigned SICard::calculateHash() const {
  unsigned h = nPunch * 100000 + FinishPunch.Time;
  for (unsigned i = 0; i < nPunch; i++) {
    h = h * 31 + Punch[i].Code;
    h = h * 31 + Punch[i].Time;
  }
  h += StartPunch.Time;
  return h;
}

string SICard::serializePunches() const {
  string ser;
  if (CheckPunch.Code != -1)
    ser += "C-" + itos(CheckPunch.Time);
  
  if (StartPunch.Code != -1) {
    if (!ser.empty()) ser += ";";
    ser += "S-" + itos(StartPunch.Time);
  }
  for (DWORD i = 0; i < nPunch; i++) {
    if (!ser.empty()) ser += ";";
    ser += itos(Punch[i].Code) + "-" + itos(Punch[i].Time);
  }

  if (FinishPunch.Code != -1) {
    if (!ser.empty()) ser += ";";
    ser += "F-" + itos(FinishPunch.Time);
  }
  return ser;
}

void SICard::deserializePunches(const string &arg) {
  FinishPunch.Code = -1;
  StartPunch.Code = -1;
  CheckPunch.Code = -1;
  vector<string> out;
  split(arg, ";", out);
  nPunch = 0;
  for (size_t k = 0; k< out.size(); k++) {
    vector<string> mark;
    split(out[k], "-", mark);
    if (mark.size() != 2)
      throw std::exception("Invalid string");
    DWORD *tp = 0;
    if (mark[0] == "F") {
      FinishPunch.Code = 1;
      tp = &FinishPunch.Time;
    }
    else if (mark[0] == "S") {
      StartPunch.Code = 1;
      tp = &StartPunch.Time;
    }
    else if (mark[0] == "C") {
      CheckPunch.Code = 1;
      tp = &CheckPunch.Time;
    }
    else {
      Punch[nPunch].Code = atoi(mark[0].c_str());
      tp = &Punch[nPunch++].Time;
    }

    *tp = atoi(mark[1].c_str());
  }
}
