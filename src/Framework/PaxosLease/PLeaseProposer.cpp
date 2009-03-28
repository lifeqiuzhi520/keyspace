#include "PLeaseProposer.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "System/Log.h"
#include "System/Events/EventLoop.h"
#include "Framework/Database/Transaction.h"
#include "Framework/ReplicatedLog/ReplicatedLog.h"
#include "Framework/Paxos/PaxosConfig.h"
#include "PLeaseConsts.h"

PLeaseProposer::PLeaseProposer() :
	onAcquireLeaseTimeout(this, &PLeaseProposer::OnAcquireLeaseTimeout),
	acquireLeaseTimeout(MAX_LEASE_TIME, &onAcquireLeaseTimeout),
	onExtendLeaseTimeout(this, &PLeaseProposer::OnExtendLeaseTimeout),
	extendLeaseTimeout(&onExtendLeaseTimeout)
{
}

void PLeaseProposer::Init(TransportWriter** writers_)
{
	writers = writers_;
	
	state.Init();
}

void PLeaseProposer::AcquireLease()
{
	EventLoop::Get()->Remove(&extendLeaseTimeout);
	
	if (!(state.preparing || state.proposing))
		StartPreparing();
}

void PLeaseProposer::BroadcastMessage()
{
	Log_Trace();
	
	numReceived = 0;
	numAccepted = 0;
	numRejected = 0;
	
	msg.Write(wdata);
	
	for (unsigned nodeID = 0; nodeID < PaxosConfig::Get()->numNodes; nodeID++)
		writers[nodeID]->Write(wdata);
}

void PLeaseProposer::ProcessMsg(PLeaseMsg &msg_)
{
	msg = msg_;

	if (msg.type == PREPARE_RESPONSE)
		OnPrepareResponse();
	else if (msg.type == PROPOSE_RESPONSE)
		OnProposeResponse();
	else
		ASSERT_FAIL();
}

void PLeaseProposer::OnPrepareResponse()
{
	Log_Trace();

	if (state.expireTime < Now())
		return; // wait for timer

	if (!state.preparing || msg.proposalID != state.proposalID)
		return;
		
	numReceived++;
	
	if (msg.response == PREPARE_REJECTED)
		numRejected++;
	else if (msg.response == PREPARE_PREVIOUSLY_ACCEPTED && 
			 msg.acceptedProposalID >= state.highestReceivedProposalID &&
			 msg.expireTime > Now())
	{
		state.highestReceivedProposalID = msg.acceptedProposalID;
		state.leaseOwner = msg.leaseOwner;
	}

	if (numRejected >= ceil(PaxosConfig::Get()->numNodes / 2))
	{
		StartPreparing();
		return;
	}
	
	// see if we have enough positive replies to advance	
	if ((numReceived - numRejected) >= PaxosConfig::Get()->MinMajority())
		StartProposing();	
}

void PLeaseProposer::OnProposeResponse()
{
	Log_Trace();

	if (state.expireTime < Now())
		return; // wait for timer
	
	if (!state.proposing || msg.proposalID != state.proposalID)
		return;
	
	numReceived++;
	
	if (msg.response == PROPOSE_ACCEPTED)
		numAccepted++;

	// see if we have enough positive replies to advance
	if (numAccepted >= PaxosConfig::Get()->MinMajority())
	{
		// a majority have accepted our proposal, we have consensus
		state.proposing = false;
		msg.LearnChosen(PaxosConfig::Get()->nodeID, state.leaseOwner, state.expireTime);
		
		EventLoop::Get()->Remove(&acquireLeaseTimeout);
		
		extendLeaseTimeout.Set(Now() + (state.expireTime - Now()) / 2);
		EventLoop::Get()->Reset(&extendLeaseTimeout);
		
		BroadcastMessage();
		return;
	}
	
	if (numReceived == PaxosConfig::Get()->numNodes)
		StartPreparing();
}

void PLeaseProposer::StartPreparing()
{
	Log_Trace();

	EventLoop::Get()->Reset(&acquireLeaseTimeout);

	state.proposing = false;

	state.preparing = true;
	
	state.leaseOwner = PaxosConfig::Get()->nodeID;
	state.expireTime = Now() + MAX_LEASE_TIME;
	
	state.highestReceivedProposalID = 0;

	state.proposalID = PaxosConfig::Get()->NextHighest(state.proposalID);
		
	msg.PrepareRequest(PaxosConfig::Get()->nodeID, state.proposalID, ReplicatedLog::Get()->GetPaxosID());
	
	BroadcastMessage();
}

void PLeaseProposer::StartProposing()
{
	Log_Trace();
	
	state.preparing = false;

	if (state.leaseOwner != PaxosConfig::Get()->nodeID)
		return; // no point in getting someone else a lease, wait for OnAcquireLeaseTimeout

	state.proposing = true;

	// acceptors get (t_e + S)
	msg.ProposeRequest(PaxosConfig::Get()->nodeID, state.proposalID,
		state.leaseOwner, state.expireTime + MAX_CLOCK_SKEW);

	BroadcastMessage();
}

void PLeaseProposer::OnAcquireLeaseTimeout()
{
	Log_Trace();
		
	StartPreparing();
}

void PLeaseProposer::OnExtendLeaseTimeout()
{
	Log_Trace();
	
	assert(!(state.preparing || state.proposing));
	
	StartPreparing();
}