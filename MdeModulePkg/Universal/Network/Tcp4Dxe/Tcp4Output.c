/** @file
  TCP output process routines.
    
Copyright (c) 2005 - 2017, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php<BR>

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "Tcp4Main.h"

UINT8  mTcpOutFlag[] = {
  0,                          // TCP_CLOSED
  0,                          // TCP_LISTEN
  TCP_FLG_SYN,                // TCP_SYN_SENT
  TCP_FLG_SYN | TCP_FLG_ACK,  // TCP_SYN_RCVD
  TCP_FLG_ACK,                // TCP_ESTABLISHED
  TCP_FLG_FIN | TCP_FLG_ACK,  // TCP_FIN_WAIT_1
  TCP_FLG_ACK,                // TCP_FIN_WAIT_2
  TCP_FLG_ACK | TCP_FLG_FIN,  // TCP_CLOSING
  TCP_FLG_ACK,                // TCP_TIME_WAIT
  TCP_FLG_ACK,                // TCP_CLOSE_WAIT
  TCP_FLG_FIN | TCP_FLG_ACK   // TCP_LAST_ACK
};


/**
  Compute the sequence space left in the old receive window.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.

  @return The sequence space left in the old receive window.

**/
UINT32
TcpRcvWinOld (
  IN TCP_CB *Tcb
  )
{
  UINT32  OldWin;

  OldWin = 0;

  if (TCP_SEQ_GT (Tcb->RcvWl2 + Tcb->RcvWnd, Tcb->RcvNxt)) {

    OldWin = TCP_SUB_SEQ (
              Tcb->RcvWl2 + Tcb->RcvWnd,
              Tcb->RcvNxt
              );
  }

  return OldWin;
}


/**
  Compute the current receive window.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.

  @return The size of the current receive window, in bytes.

**/
UINT32
TcpRcvWinNow (
  IN TCP_CB *Tcb
  )
{
  SOCKET  *Sk;
  UINT32  Win;
  UINT32  Increase;
  UINT32  OldWin;

  Sk = Tcb->Sk;
  ASSERT (Sk != NULL);

  OldWin    = TcpRcvWinOld (Tcb);

  Win       = SockGetFreeSpace (Sk, SOCK_RCV_BUF);

  Increase  = 0;
  if (Win > OldWin) {
    Increase = Win - OldWin;
  }

  //
  // Receiver's SWS: don't advertise a bigger window
  // unless it can be increased by at least one Mss or
  // half of the receive buffer.
  //
  if ((Increase > Tcb->SndMss) ||
      (2 * Increase >= GET_RCV_BUFFSIZE (Sk))) {

    return Win;
  }

  return OldWin;
}


/**
  Compute the value to fill in the window size field of the outgoing segment.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.
  @param  Syn     The flag to indicate whether the outgoing segment is a SYN
                  segment.

  @return The value of the local receive window size used to fill the outing segment.

**/
UINT16
TcpComputeWnd (
  IN OUT TCP_CB  *Tcb,
  IN     BOOLEAN Syn
  )
{
  UINT32  Wnd;

  //
  // RFC requires that initial window not be scaled
  //
  if (Syn) {

    Wnd = GET_RCV_BUFFSIZE (Tcb->Sk);
  } else {

    Wnd         = TcpRcvWinNow (Tcb);

    Tcb->RcvWnd = Wnd;
  }

  Wnd = MIN (Wnd >> Tcb->RcvWndScale, 0xffff);
  return NTOHS ((UINT16) Wnd);
}


/**
  Get the maximum SndNxt.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.

  @return The sequence number of the maximum SndNxt.

**/
TCP_SEQNO
TcpGetMaxSndNxt (
  IN TCP_CB *Tcb
  )
{
  LIST_ENTRY      *Entry;
  NET_BUF         *Nbuf;

  if (IsListEmpty (&Tcb->SndQue)) {
    return Tcb->SndNxt;
  }

  Entry = Tcb->SndQue.BackLink;
  Nbuf  = NET_LIST_USER_STRUCT (Entry, NET_BUF, List);

  ASSERT (TCP_SEQ_GEQ (TCPSEG_NETBUF (Nbuf)->End, Tcb->SndNxt));
  return TCPSEG_NETBUF (Nbuf)->End;
}


/**
  Compute how much data to send.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.
  @param  Force   Whether to ignore the sender's SWS avoidance algorithm and send
                  out data by force.

  @return The length of the data can be sent, if 0, no data can be sent.

**/
UINT32
TcpDataToSend (
  IN TCP_CB *Tcb,
  IN INTN   Force
  )
{
  SOCKET  *Sk;
  UINT32  Win;
  UINT32  Len;
  UINT32  Left;
  UINT32  Limit;

  Sk = Tcb->Sk;
  ASSERT (Sk != NULL);

  //
  // TCP should NOT send data beyond the send window
  // and congestion window. The right edge of send
  // window is defined as SND.WL2 + SND.WND. The right
  // edge of congestion window is defined as SND.UNA +
  // CWND.
  //
  Win   = 0;
  Limit = Tcb->SndWl2 + Tcb->SndWnd;

  if (TCP_SEQ_GT (Limit, Tcb->SndUna + Tcb->CWnd)) {

    Limit = Tcb->SndUna + Tcb->CWnd;
  }

  if (TCP_SEQ_GT (Limit, Tcb->SndNxt)) {
    Win = TCP_SUB_SEQ (Limit, Tcb->SndNxt);
  }

  //
  // The data to send contains two parts: the data on the
  // socket send queue, and the data on the TCB's send
  // buffer. The later can be non-zero if the peer shrinks
  // its advertised window.
  //
  Left  = GET_SND_DATASIZE (Sk) +
          TCP_SUB_SEQ (TcpGetMaxSndNxt (Tcb), Tcb->SndNxt);

  Len   = MIN (Win, Left);

  if (Len > Tcb->SndMss) {
    Len = Tcb->SndMss;
  }

  if ((Force != 0)|| (Len == 0 && Left == 0)) {
    return Len;
  }

  if (Len == 0 && Left != 0) {
    goto SetPersistTimer;
  }

  //
  // Sender's SWS avoidance: Don't send a small segment unless
  // a)A full-sized segment can be sent,
  // b)at least one-half of the maximum sized windows that
  // the other end has ever advertised.
  // c)It can send everything it has and either it isn't
  // expecting an ACK or the Nagle algorithm is disabled.
  //
  if ((Len == Tcb->SndMss) || (2 * Len >= Tcb->SndWndMax)) {

    return Len;
  }

  if ((Len == Left) &&
      ((Tcb->SndNxt == Tcb->SndUna) ||
      TCP_FLG_ON (Tcb->CtrlFlag, TCP_CTRL_NO_NAGLE))) {

    return Len;
  }

  //
  // RFC1122 suggests to set a timer when SWSA forbids TCP
  // sending more data, and combine it with probe timer.
  //
SetPersistTimer:
  if (!TCP_TIMER_ON (Tcb->EnabledTimer, TCP_TIMER_REXMIT)) {

    DEBUG (
      (EFI_D_WARN,
      "TcpDataToSend: enter persistent state for TCB %p\n",
      Tcb)
      );

    if (!Tcb->ProbeTimerOn) {
      TcpSetProbeTimer (Tcb);
    }
  }

  return 0;
}


/**
  Build the TCP header of the TCP segment and transmit the segment by IP.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.
  @param  Nbuf    Pointer to the buffer containing the segment to be sent out.

  @retval 0       The segment is sent out successfully.
  @retval other   Error condition occurred.

**/
INTN
TcpTransmitSegment (
  IN OUT TCP_CB  *Tcb,
  IN     NET_BUF *Nbuf
  )
{
  UINT16    Len;
  TCP_HEAD  *Head;
  TCP_SEG   *Seg;
  BOOLEAN   Syn;
  UINT32    DataLen;

  ASSERT ((Nbuf != NULL) && (Nbuf->Tcp == NULL) && (TcpVerifySegment (Nbuf) != 0));

  DataLen = Nbuf->TotalSize;

  Seg     = TCPSEG_NETBUF (Nbuf);
  Syn     = TCP_FLG_ON (Seg->Flag, TCP_FLG_SYN);

  if (Syn) {

    Len = TcpSynBuildOption (Tcb, Nbuf);
  } else {

    Len = TcpBuildOption (Tcb, Nbuf);
  }

  ASSERT ((Len % 4 == 0) && (Len <= 40));

  Len += sizeof (TCP_HEAD);

  Head = (TCP_HEAD *) NetbufAllocSpace (
                        Nbuf,
                        sizeof (TCP_HEAD),
                        NET_BUF_HEAD
                        );

  ASSERT (Head != NULL);

  Nbuf->Tcp       = Head;

  Head->SrcPort   = Tcb->LocalEnd.Port;
  Head->DstPort   = Tcb->RemoteEnd.Port;
  Head->Seq       = NTOHL (Seg->Seq);
  Head->Ack       = NTOHL (Tcb->RcvNxt);
  Head->HeadLen   = (UINT8) (Len >> 2);
  Head->Res       = 0;
  Head->Wnd       = TcpComputeWnd (Tcb, Syn);
  Head->Checksum  = 0;

  //
  // Check whether to set the PSH flag.
  //
  TCP_CLEAR_FLG (Seg->Flag, TCP_FLG_PSH);

  if (DataLen != 0) {
    if (TCP_FLG_ON (Tcb->CtrlFlag, TCP_CTRL_SND_PSH) &&
        TCP_SEQ_BETWEEN (Seg->Seq, Tcb->SndPsh, Seg->End)) {

      TCP_SET_FLG (Seg->Flag, TCP_FLG_PSH);
      TCP_CLEAR_FLG (Tcb->CtrlFlag, TCP_CTRL_SND_PSH);

    } else if ((Seg->End == Tcb->SndNxt) &&
               (GET_SND_DATASIZE (Tcb->Sk) == 0)) {

      TCP_SET_FLG (Seg->Flag, TCP_FLG_PSH);
    }
  }

  //
  // Check whether to set the URG flag and the urgent pointer.
  //
  TCP_CLEAR_FLG (Seg->Flag, TCP_FLG_URG);

  if (TCP_FLG_ON (Tcb->CtrlFlag, TCP_CTRL_SND_URG) &&
      TCP_SEQ_LEQ (Seg->Seq, Tcb->SndUp)) {

    TCP_SET_FLG (Seg->Flag, TCP_FLG_URG);

    if (TCP_SEQ_LT (Tcb->SndUp, Seg->End)) {

      Seg->Urg = (UINT16) TCP_SUB_SEQ (Tcb->SndUp, Seg->Seq);
    } else {

      Seg->Urg = (UINT16) MIN (
                            TCP_SUB_SEQ (Tcb->SndUp,
                            Seg->Seq),
                            0xffff
                            );
    }
  }

  Head->Flag      = Seg->Flag;
  Head->Urg       = NTOHS (Seg->Urg);
  Head->Checksum  = TcpChecksum (Nbuf, Tcb->HeadSum);

  //
  // update the TCP session's control information
  //
  Tcb->RcvWl2 = Tcb->RcvNxt;
  if (Syn) {
    Tcb->RcvWnd = NTOHS (Head->Wnd);
  }

  //
  // clear delayedack flag
  //
  Tcb->DelayedAck = 0;

  return TcpSendIpPacket (Tcb, Nbuf, Tcb->LocalEnd.Ip, Tcb->RemoteEnd.Ip);
}


/**
  Get a segment from the Tcb's SndQue.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.
  @param  Seq     The sequence number of the segment.
  @param  Len     The maximum length of the segment.

  @return Pointer to the segment, if NULL some error occurred.

**/
NET_BUF *
TcpGetSegmentSndQue (
  IN TCP_CB    *Tcb,
  IN TCP_SEQNO Seq,
  IN UINT32    Len
  )
{
  LIST_ENTRY      *Head;
  LIST_ENTRY      *Cur;
  NET_BUF         *Node;
  TCP_SEG         *Seg;
  NET_BUF         *Nbuf;
  TCP_SEQNO       End;
  UINT8           *Data;
  UINT8           Flag;
  INT32           Offset;
  INT32           CopyLen;

  ASSERT ((Tcb != NULL) && TCP_SEQ_LEQ (Seq, Tcb->SndNxt) && (Len > 0));

  //
  // Find the segment that contains the Seq.
  //
  Head  = &Tcb->SndQue;

  Node  = NULL;
  Seg   = NULL;

  NET_LIST_FOR_EACH (Cur, Head) {
    Node  = NET_LIST_USER_STRUCT (Cur, NET_BUF, List);
    Seg   = TCPSEG_NETBUF (Node);

    if (TCP_SEQ_LT (Seq, Seg->End) && TCP_SEQ_LEQ (Seg->Seq, Seq)) {

      break;
    }
  }

  ASSERT (Cur  != Head);
  ASSERT (Node != NULL);
  ASSERT (Seg  != NULL);

  //
  // Return the buffer if it can be returned without
  // adjustment:
  //
  if ((Seg->Seq == Seq) &&
      TCP_SEQ_LEQ (Seg->End, Seg->Seq + Len) &&
      !NET_BUF_SHARED (Node)) {

    NET_GET_REF (Node);
    return Node;
  }

  //
  // Create a new buffer and copy data there.
  //
  Nbuf = NetbufAlloc (Len + TCP_MAX_HEAD);

  if (Nbuf == NULL) {
    return NULL;
  }

  NetbufReserve (Nbuf, TCP_MAX_HEAD);

  Flag  = Seg->Flag;
  End   = Seg->End;

  if (TCP_SEQ_LT (Seq + Len, Seg->End)) {
    End = Seq + Len;
  }

  CopyLen = TCP_SUB_SEQ (End, Seq);
  Offset  = TCP_SUB_SEQ (Seq, Seg->Seq);

  //
  // If SYN is set and out of the range, clear the flag.
  // Because the sequence of the first byte is SEG.SEQ+1,
  // adjust Offset by -1. If SYN is in the range, copy
  // one byte less.
  //
  if (TCP_FLG_ON (Seg->Flag, TCP_FLG_SYN)) {

    if (TCP_SEQ_LT (Seg->Seq, Seq)) {

      TCP_CLEAR_FLG (Flag, TCP_FLG_SYN);
      Offset--;
    } else {

      CopyLen--;
    }
  }

  //
  // If FIN is set and in the range, copy one byte less,
  // and if it is out of the range, clear the flag.
  //
  if (TCP_FLG_ON (Seg->Flag, TCP_FLG_FIN)) {

    if (Seg->End == End) {

      CopyLen--;
    } else {

      TCP_CLEAR_FLG (Flag, TCP_FLG_FIN);
    }
  }

  ASSERT (CopyLen >= 0);

  //
  // copy data to the segment
  //
  if (CopyLen != 0) {
    Data = NetbufAllocSpace (Nbuf, CopyLen, NET_BUF_TAIL);
    ASSERT (Data != NULL);

    if ((INT32) NetbufCopy (Node, Offset, CopyLen, Data) != CopyLen) {
      goto OnError;
    }
  }

  CopyMem (TCPSEG_NETBUF (Nbuf), Seg, sizeof (TCP_SEG));

  TCPSEG_NETBUF (Nbuf)->Seq   = Seq;
  TCPSEG_NETBUF (Nbuf)->End   = End;
  TCPSEG_NETBUF (Nbuf)->Flag  = Flag;

  return Nbuf;

OnError:
  NetbufFree (Nbuf);
  return NULL;
}


/**
  Get a segment from the Tcb's socket buffer.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.
  @param  Seq     The sequence number of the segment.
  @param  Len     The maximum length of the segment.

  @return Pointer to the segment, if NULL some error occurred.

**/
NET_BUF *
TcpGetSegmentSock (
  IN TCP_CB    *Tcb,
  IN TCP_SEQNO Seq,
  IN UINT32    Len
  )
{
  NET_BUF *Nbuf;
  UINT8   *Data;
  UINT32  DataGet;

  ASSERT ((Tcb != NULL) && (Tcb->Sk != NULL));

  Nbuf = NetbufAlloc (Len + TCP_MAX_HEAD);

  if (Nbuf == NULL) {
    DEBUG ((EFI_D_ERROR, "TcpGetSegmentSock: failed to allocate "
      "a netbuf for TCB %p\n",Tcb));

    return NULL;
  }

  NetbufReserve (Nbuf, TCP_MAX_HEAD);

  DataGet = 0;

  if (Len != 0) {
    //
    // copy data to the segment.
    //
    Data = NetbufAllocSpace (Nbuf, Len, NET_BUF_TAIL);
    ASSERT (Data != NULL);

    DataGet = SockGetDataToSend (Tcb->Sk, 0, Len, Data);
  }

  NET_GET_REF (Nbuf);

  TCPSEG_NETBUF (Nbuf)->Seq = Seq;
  TCPSEG_NETBUF (Nbuf)->End = Seq + Len;

  InsertTailList (&(Tcb->SndQue), &(Nbuf->List));

  if (DataGet != 0) {

    SockDataSent (Tcb->Sk, DataGet);
  }

  return Nbuf;
}


/**
  Get a segment starting from sequence Seq of a maximum
  length of Len.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.
  @param  Seq     The sequence number of the segment.
  @param  Len     The maximum length of the segment.

  @return Pointer to the segment, if NULL some error occurred.

**/
NET_BUF *
TcpGetSegment (
  IN TCP_CB    *Tcb,
  IN TCP_SEQNO Seq,
  IN UINT32    Len
  )
{
  NET_BUF *Nbuf;

  ASSERT (Tcb != NULL);

  //
  // Compare the SndNxt with the max sequence number sent.
  //
  if ((Len != 0) && TCP_SEQ_LT (Seq, TcpGetMaxSndNxt (Tcb))) {

    Nbuf = TcpGetSegmentSndQue (Tcb, Seq, Len);
  } else {

    Nbuf = TcpGetSegmentSock (Tcb, Seq, Len);
  }

  ASSERT (TcpVerifySegment (Nbuf) != 0);
  return Nbuf;
}


/**
  Retransmit the segment from sequence Seq.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.
  @param  Seq     The sequence number of the segment to be retransmitted.

  @retval 0       Retransmission succeeded.
  @retval -1      Error condition occurred.

**/
INTN
TcpRetransmit (
  IN TCP_CB    *Tcb,
  IN TCP_SEQNO Seq
  )
{
  NET_BUF *Nbuf;
  UINT32  Len;

  //
  // Compute the maximum length of retransmission. It is
  // limited by three factors:
  // 1. Less than SndMss
  // 2. must in the current send window
  // 3. will not change the boundaries of queued segments.
  //

  //
  // Handle the Window Retraction if TCP window scale is enabled according to RFC7323:
  //   On first retransmission, or if the sequence number is out of
  //   window by less than 2^Rcv.Wind.Shift, then do normal
  //   retransmission(s) without regard to the receiver window as long
  //   as the original segment was in window when it was sent.
  //
  if ((Tcb->SndWndScale != 0) &&
      (TCP_SEQ_GT (Seq, Tcb->RetxmitSeqMax) || TCP_SEQ_BETWEEN (Tcb->SndWl2 + Tcb->SndWnd, Seq, Tcb->SndWl2 + Tcb->SndWnd + (1 << Tcb->SndWndScale)))) {
    Len = TCP_SUB_SEQ (Tcb->SndNxt, Seq);
    DEBUG (
      (EFI_D_WARN,
      "TcpRetransmit: retransmission without regard to the receiver window for TCB %p\n",
      Tcb)
      );
    
  } else if (TCP_SEQ_GEQ (Tcb->SndWl2 + Tcb->SndWnd, Seq)) {
    Len = TCP_SUB_SEQ (Tcb->SndWl2 + Tcb->SndWnd, Seq);
    
  } else {
    DEBUG (
      (EFI_D_WARN,
      "TcpRetransmit: retransmission cancelled because send window too small for TCB %p\n",
      Tcb)
      );

    return 0;
  }
  
  Len = MIN (Len, Tcb->SndMss);

  Nbuf = TcpGetSegmentSndQue (Tcb, Seq, Len);
  if (Nbuf == NULL) {
    return -1;
  }

  ASSERT (TcpVerifySegment (Nbuf) != 0);

  if (TcpTransmitSegment (Tcb, Nbuf) != 0) {
    goto OnError;
  }
  
  if (TCP_SEQ_GT (Seq, Tcb->RetxmitSeqMax)) {
    Tcb->RetxmitSeqMax = Seq;
  }

  //
  // The retransmitted buffer may be on the SndQue,
  // trim TCP head because all the buffer on SndQue
  // are headless.
  //
  ASSERT (Nbuf->Tcp != NULL);
  NetbufTrim (Nbuf, (Nbuf->Tcp->HeadLen << 2), NET_BUF_HEAD);
  Nbuf->Tcp = NULL;

  NetbufFree (Nbuf);
  return 0;

OnError:
  if (Nbuf != NULL) {
    NetbufFree (Nbuf);
  }

  return -1;
}


/**
  Check whether to send data/SYN/FIN and piggy back an ACK.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.
  @param  Force   Whether to ignore the sender's SWS avoidance algorithm and send
                  out data by force.

  @return The number of bytes sent.

**/
INTN
TcpToSendData (
  IN OUT TCP_CB *Tcb,
  IN     INTN Force
  )
{
  UINT32    Len;
  INTN      Sent;
  UINT8     Flag;
  NET_BUF   *Nbuf;
  TCP_SEG   *Seg;
  TCP_SEQNO Seq;
  TCP_SEQNO End;

  ASSERT ((Tcb != NULL) && (Tcb->Sk != NULL) && (Tcb->State != TCP_LISTEN));

  Sent = 0;

  if ((Tcb->State == TCP_CLOSED) ||
      TCP_FLG_ON (Tcb->CtrlFlag, TCP_CTRL_FIN_SENT)) {

    return 0;
  }

SEND_AGAIN:
  //
  // compute how much data can be sent
  //
  Len   = TcpDataToSend (Tcb, Force);
  Seq   = Tcb->SndNxt;

  ASSERT ((Tcb->State) < (ARRAY_SIZE (mTcpOutFlag)));
  Flag  = mTcpOutFlag[Tcb->State];

  if ((Flag & TCP_FLG_SYN) != 0) {

    Seq = Tcb->Iss;
    Len = 0;
  }

  //
  // only send a segment without data if SYN or
  // FIN is set.
  //
  if ((Len == 0) && 
      ((Flag & (TCP_FLG_SYN | TCP_FLG_FIN)) == 0)) {
    return Sent;
  }

  Nbuf = TcpGetSegment (Tcb, Seq, Len);

  if (Nbuf == NULL) {
    DEBUG (
      (EFI_D_ERROR,
      "TcpToSendData: failed to get a segment for TCB %p\n",
      Tcb)
      );

    goto OnError;
  }

  Seg = TCPSEG_NETBUF (Nbuf);

  //
  // Set the TcpSeg in Nbuf.
  //
  Len = Nbuf->TotalSize;
  End = Seq + Len;
  if (TCP_FLG_ON (Flag, TCP_FLG_SYN)) {
    End++;
  }

  if ((Flag & TCP_FLG_FIN) != 0) {
    //
    // Send FIN if all data is sent, and FIN is
    // in the window
    //
    if ((TcpGetMaxSndNxt (Tcb) == Tcb->SndNxt) &&
        (GET_SND_DATASIZE (Tcb->Sk) == 0) &&
        TCP_SEQ_LT (End + 1, Tcb->SndWnd + Tcb->SndWl2)) {

      DEBUG (
	  	(EFI_D_NET, 
	  	"TcpToSendData: send FIN "
        "to peer for TCB %p in state %s\n", 
        Tcb, 
        mTcpStateName[Tcb->State])
      );

      End++;
    } else {
      TCP_CLEAR_FLG (Flag, TCP_FLG_FIN);
    }
  }

  Seg->Seq  = Seq;
  Seg->End  = End;
  Seg->Flag = Flag;

  ASSERT (TcpVerifySegment (Nbuf) != 0);
  ASSERT (TcpCheckSndQue (&Tcb->SndQue) != 0);

  //
  // don't send an empty segment here.
  //
  if (Seg->End == Seg->Seq) {
    DEBUG ((EFI_D_WARN, "TcpToSendData: created a empty"
      " segment for TCB %p, free it now\n", Tcb));

    NetbufFree (Nbuf);
    return Sent;
  }

  if (TcpTransmitSegment (Tcb, Nbuf) != 0) {
    NetbufTrim (Nbuf, (Nbuf->Tcp->HeadLen << 2), NET_BUF_HEAD);
    Nbuf->Tcp = NULL;

    if ((Flag & TCP_FLG_FIN) != 0)  {
      TCP_SET_FLG (Tcb->CtrlFlag, TCP_CTRL_FIN_SENT);
    }

    goto OnError;
  }

  Sent += TCP_SUB_SEQ (End, Seq);

  //
  // All the buffer in the SndQue is headless
  //
  ASSERT (Nbuf->Tcp != NULL);

  NetbufTrim (Nbuf, (Nbuf->Tcp->HeadLen << 2), NET_BUF_HEAD);
  Nbuf->Tcp = NULL;

  NetbufFree (Nbuf);

  //
  // update status in TCB
  //
  Tcb->DelayedAck = 0;

  if ((Flag & TCP_FLG_FIN) != 0) {
    TCP_SET_FLG (Tcb->CtrlFlag, TCP_CTRL_FIN_SENT);
  }

  if (TCP_SEQ_GT (End, Tcb->SndNxt)) {
    Tcb->SndNxt = End;
  }

  if (!TCP_TIMER_ON (Tcb->EnabledTimer, TCP_TIMER_REXMIT)) {
    TcpSetTimer (Tcb, TCP_TIMER_REXMIT, Tcb->Rto);
  }

  //
  // Enable RTT measurement only if not in retransmit.
  // Karn's algorithm reqires not to update RTT when in loss.
  //
  if ((Tcb->CongestState == TCP_CONGEST_OPEN) &&
      !TCP_FLG_ON (Tcb->CtrlFlag, TCP_CTRL_RTT_ON)) {

    DEBUG ((EFI_D_NET, "TcpToSendData: set RTT measure "
      "sequence %d for TCB %p\n", Seq, Tcb));

    TCP_SET_FLG (Tcb->CtrlFlag, TCP_CTRL_RTT_ON);
    Tcb->RttSeq     = Seq;
    Tcb->RttMeasure = 0;
  }

  if (Len == Tcb->SndMss) {
    goto SEND_AGAIN;
  }

  return Sent;

OnError:
  if (Nbuf != NULL) {
    NetbufFree (Nbuf);
  }

  return Sent;
}


/**
  Send an ACK immediately.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.

**/
VOID
TcpSendAck (
  IN OUT TCP_CB *Tcb
  )
{
  NET_BUF *Nbuf;
  TCP_SEG *Seg;

  Nbuf = NetbufAlloc (TCP_MAX_HEAD);

  if (Nbuf == NULL) {
    return;
  }

  NetbufReserve (Nbuf, TCP_MAX_HEAD);

  Seg       = TCPSEG_NETBUF (Nbuf);
  Seg->Seq  = Tcb->SndNxt;
  Seg->End  = Tcb->SndNxt;
  Seg->Flag = TCP_FLG_ACK;

  if (TcpTransmitSegment (Tcb, Nbuf) == 0) {
    TCP_CLEAR_FLG (Tcb->CtrlFlag, TCP_CTRL_ACK_NOW);
    Tcb->DelayedAck = 0;
  }

  NetbufFree (Nbuf);
}


/**
  Send a zero probe segment. It can be used by keepalive and zero window probe.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.

  @retval 0       The zero probe segment was sent out successfully.
  @retval other   Error condition occurred.

**/
INTN
TcpSendZeroProbe (
  IN OUT TCP_CB *Tcb
  )
{
  NET_BUF *Nbuf;
  TCP_SEG *Seg;
  INTN     Result;

  Nbuf = NetbufAlloc (TCP_MAX_HEAD);

  if (Nbuf == NULL) {
    return -1;
  }

  NetbufReserve (Nbuf, TCP_MAX_HEAD);

  //
  // SndNxt-1 is out of window. The peer should respond
  // with an ACK.
  //
  Seg       = TCPSEG_NETBUF (Nbuf);
  Seg->Seq  = Tcb->SndNxt - 1;
  Seg->End  = Tcb->SndNxt - 1;
  Seg->Flag = TCP_FLG_ACK;

  Result    = TcpTransmitSegment (Tcb, Nbuf);
  NetbufFree (Nbuf);

  return Result;
}


/**
  Check whether to send an ACK or delayed ACK.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance.

**/
VOID
TcpToSendAck (
  IN OUT TCP_CB *Tcb
  )
{
  UINT32 TcpNow;

  TcpNow = TcpRcvWinNow (Tcb);
  //
  // Generally, TCP should send a delayed ACK unless:
  //   1. ACK at least every other FULL sized segment received,
  //   2. Packets received out of order
  //   3. Receiving window is open
  //
  if (TCP_FLG_ON (Tcb->CtrlFlag, TCP_CTRL_ACK_NOW) ||
      (Tcb->DelayedAck >= 1) ||
      (TcpNow > TcpRcvWinOld (Tcb))) {
    TcpSendAck (Tcb);
    return;
  }

  DEBUG ((EFI_D_NET, "TcpToSendAck: scheduled a delayed"
    " ACK for TCB %p\n", Tcb));

  //
  // schedule a delayed ACK
  //
  Tcb->DelayedAck++;
}


/**
  Send a RESET segment in response to the segment received.

  @param  Tcb     Pointer to the TCP_CB of this TCP instance, may be NULL.
  @param  Head    TCP header of the segment that triggers the reset.
  @param  Len     Length of the segment that triggers the reset.
  @param  Local   Local IP address.
  @param  Remote  Remote peer's IP address.

  @retval 0       A reset is sent or no need to send it.
  @retval -1      No reset is sent.

**/
INTN
TcpSendReset (
  IN TCP_CB    *Tcb,
  IN TCP_HEAD  *Head,
  IN INT32     Len,
  IN UINT32    Local,
  IN UINT32    Remote
  )
{
  NET_BUF   *Nbuf;
  TCP_HEAD  *Nhead;
  UINT16    HeadSum;

  //
  // Don't respond to a Reset with reset
  //
  if ((Head->Flag & TCP_FLG_RST) != 0) {
    return 0;
  }

  Nbuf = NetbufAlloc (TCP_MAX_HEAD);

  if (Nbuf == NULL) {
    return -1;
  }

  Nhead = (TCP_HEAD *) NetbufAllocSpace (
                        Nbuf,
                        sizeof (TCP_HEAD),
                        NET_BUF_TAIL
                        );

  ASSERT (Nhead != NULL);

  Nbuf->Tcp   = Nhead;
  Nhead->Flag = TCP_FLG_RST;

  //
  // Derive Seq/ACK from the segment if no TCB
  // associated with it, otherwise from the Tcb
  //
  if (Tcb == NULL) {

    if (TCP_FLG_ON (Head->Flag, TCP_FLG_ACK)) {
      Nhead->Seq  = Head->Ack;
      Nhead->Ack  = 0;
    } else {
      Nhead->Seq = 0;
      TCP_SET_FLG (Nhead->Flag, TCP_FLG_ACK);
      Nhead->Ack = HTONL (NTOHL (Head->Seq) + Len);
    }
  } else {

    Nhead->Seq  = HTONL (Tcb->SndNxt);
    Nhead->Ack  = HTONL (Tcb->RcvNxt);
    TCP_SET_FLG (Nhead->Flag, TCP_FLG_ACK);
  }

  Nhead->SrcPort  = Head->DstPort;
  Nhead->DstPort  = Head->SrcPort;
  Nhead->HeadLen  = (UINT8) (sizeof (TCP_HEAD) >> 2);
  Nhead->Res      = 0;
  Nhead->Wnd      = HTONS (0xFFFF);
  Nhead->Checksum = 0;
  Nhead->Urg      = 0;

  HeadSum         = NetPseudoHeadChecksum (Local, Remote, 6, 0);
  Nhead->Checksum = TcpChecksum (Nbuf, HeadSum);

  TcpSendIpPacket (Tcb, Nbuf, Local, Remote);

  NetbufFree (Nbuf);
  return 0;
}


/**
  Verify that the segment is in good shape.

  @param  Nbuf    Buffer that contains the segment to be checked.

  @retval 0       The segment is broken.
  @retval 1       The segment is in good shape.

**/
INTN
TcpVerifySegment (
  IN NET_BUF *Nbuf
  )
{
  TCP_HEAD  *Head;
  TCP_SEG   *Seg;
  UINT32    Len;

  if (Nbuf == NULL) {
    return 1;
  }

  NET_CHECK_SIGNATURE (Nbuf, NET_BUF_SIGNATURE);

  Seg   = TCPSEG_NETBUF (Nbuf);
  Len   = Nbuf->TotalSize;
  Head  = Nbuf->Tcp;

  if (Head != NULL) {
    if (Head->Flag != Seg->Flag) {
      return 0;
    }

    Len -= (Head->HeadLen << 2);
  }

  if (TCP_FLG_ON (Seg->Flag, TCP_FLG_SYN)) {
    Len++;
  }

  if (TCP_FLG_ON (Seg->Flag, TCP_FLG_FIN)) {
    Len++;
  }

  if (Seg->Seq + Len != Seg->End) {
    return 0;
  }

  return 1;
}


/**
  Verify that all the segments in SndQue are in good shape.

  @param  Head    Pointer to the head node of the SndQue.

  @retval 0       At least one segment is broken.
  @retval 1       All segments in the specific queue are in good shape.

**/
INTN
TcpCheckSndQue (
  IN LIST_ENTRY     *Head
  )
{
  LIST_ENTRY      *Entry;
  NET_BUF         *Nbuf;
  TCP_SEQNO       Seq;

  if (IsListEmpty (Head)) {
    return 1;
  }
  //
  // Initialize the Seq
  //
  Entry = Head->ForwardLink;
  Nbuf  = NET_LIST_USER_STRUCT (Entry, NET_BUF, List);
  Seq   = TCPSEG_NETBUF (Nbuf)->Seq;

  NET_LIST_FOR_EACH (Entry, Head) {
    Nbuf = NET_LIST_USER_STRUCT (Entry, NET_BUF, List);

    if (TcpVerifySegment (Nbuf) == 0) {
      return 0;
    }

    //
    // All the node in the SndQue should has:
    // SEG.SEQ = LAST_SEG.END
    //
    if (Seq != TCPSEG_NETBUF (Nbuf)->Seq) {
      return 0;
    }

    Seq = TCPSEG_NETBUF (Nbuf)->End;
  }

  return 1;
}
