/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	* This function is partially implemented and may require changes
	*/
	//int id = *(int*)(&memberNode->addr.addr);
	//int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
	// node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = TREMOVE;
	initMemberListTable(memberNode);

	return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {

#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
			sendMessage(JOINREQ, joinaddr);
#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
	/*
	* Your code goes here
	*/
	return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
	MessageHdr *msg = (MessageHdr *)data;
	HeartbeatElement *hbe = (HeartbeatElement *)(msg + 1);
	Address *srcAddr = (Address *)hbe;
	for (int i = 0; i < msg->n; i++) {
		addMember(hbe->id, hbe->port, hbe->heartbeat);
		hbe++;
	}

	if (msg->msgType == JOINREQ) {
		sendMessage(JOINREP, srcAddr);
	}
	if (msg->msgType == JOINREP) {
		memberNode->inGroup = true;
	}
	return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
	/*
	* Your code goes here
	*/
	memberNode->heartbeat++;

	Address dstAddr;
	std::random_shuffle ( memberNode->memberList.begin(), memberNode->memberList.end() );
	for (int i = 0; i < 3; ++i) {
		dstAddr = getAddress(memberNode->memberList[i].id, memberNode->memberList[i].port);
		sendMessage(HEARTBEAT, &dstAddr);
	}

	for (vector<MemberListEntry>::iterator m = memberNode->memberList.begin(); m != memberNode->memberList.end(); ) {
		if (par->getcurrtime() - m->timestamp > memberNode->timeOutCounter ) {
			dstAddr = getAddress(m->id, m->port);
			log->logNodeRemove(&(memberNode->addr), &dstAddr);

			m = memberNode->memberList.erase(m);
		} else {
			++m;
		}
	}
	return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: sendMessage
 *
 * DESCRIPTION: Send a message
 */
int MP1Node::sendMessage(enum MsgTypes msgType, Address *dstAddr) {

	MessageHdr *msg;
	HeartbeatElement *hbe;
	int n = 1;
	if (msgType != JOINREQ) {
		n += memberNode->memberList.size();
	}

	size_t msgsize = sizeof(MessageHdr) + n * sizeof(HeartbeatElement);
	msg = (MessageHdr *) malloc(msgsize);
	msg->msgType = msgType;
	msg->n = n;
	hbe = (HeartbeatElement *)(msg + 1);
	hbe->id = *(int *)(&memberNode->addr.addr);
	hbe->port = *(short *)(&memberNode->addr.addr[4]);
	hbe->heartbeat = memberNode->heartbeat;
	if (msgType != JOINREQ) {
		for (vector<MemberListEntry>::iterator m = memberNode->memberList.begin(); m != memberNode->memberList.end(); ++m) {
			hbe++;
			if (par->getcurrtime() - m->timestamp <= memberNode->pingCounter ) {
				hbe->id = m->id;
				hbe->port = m->port;
				hbe->heartbeat = m->heartbeat;
			} else { // replace expired entries with own information
				hbe->id = *(int *)(&memberNode->addr.addr);
				hbe->port = *(short *)(&memberNode->addr.addr[4]);
				hbe->heartbeat = memberNode->heartbeat;
			}
		}
	}

	emulNet->ENsend(&memberNode->addr, dstAddr, (char *)msg, msgsize);
	free(msg);
	return 0;
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: getAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator base on id and port
 */
Address MP1Node::getAddress(int id, short port) {
    Address a;

    memset(&a, 0, sizeof(Address));
    *(int *)(&a.addr) = id;
    *(short *)(&a.addr[4]) = port;

    return a;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: addMember
 *
 * DESCRIPTION: Add a member to the membership list
 */
void MP1Node::addMember(int id, short port, long heartbeat) {

	// first check this member already exists
	if (!updateMember(id, port, heartbeat)) {
		// add this new member
		MemberListEntry *newMember = new MemberListEntry(id, port, heartbeat, par->getcurrtime());
		memberNode->memberList.insert(memberNode->memberList.begin(), *newMember);

		Address *srcAddr = (Address *)malloc(sizeof(Address));
		*(int *)(&srcAddr->addr[0]) = id;
		*(short *)(&srcAddr->addr[4]) = port;
		log->logNodeAdd(&(memberNode->addr), srcAddr);
	}

}

/**
 * FUNCTION NAME: updateMember
 *
 * DESCRIPTION: update a member if it exists in the membership list
 */
bool MP1Node::updateMember(int id, short port, long heartbeat) {
	for (vector<MemberListEntry>::iterator m = memberNode->memberList.begin(); m != memberNode->memberList.end(); ++m) {
		if (m->id == id && m->port == port) {
			if (m->heartbeat < heartbeat) {
				// update the heartbeat and timestamp of this existing member
				m->heartbeat = heartbeat;
				m->timestamp = par->getcurrtime();
			}
			return true;
		}
	}
	return false;
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;
}
