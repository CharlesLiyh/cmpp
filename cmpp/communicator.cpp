#include "stdafx.h"
#include "communicator.h"
#include "serialization.h"
#include "socketstream.h"
#include "AccessLock.h"
#include <utility>
using namespace std;

#pragma comment(lib, "ws2_32.lib")

namespace comm {

	//////////////////////////////////////////////////////////////////////////
	// Communicator

	Communicator::Communicator(const char* endpoint, int port){
		seqid2arrival = new map< uint32_t, pair<Arrival*, ResponseAction> >;
		givenHandlers = new map<uint32_t, GivenArrivalHandler>;
		departures = new list< tuple<uint32_t, const Departure*, ResponseAction> >;

		stream = new SocketStream(endpoint, port);
		closeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		exchangeLock = new AccessLock();
	}

	Communicator::~Communicator() {
		delete stream;
		delete departures;
		delete givenHandlers;
		delete seqid2arrival;
		delete exchangeLock;
		::CloseHandle(closeEvent);
	}

	bool Communicator::open() {
		sequenceId = 0;

		if (stream->open()) {
			::ResetEvent(closeEvent);
			departuresSemaphore = ::CreateSemaphore(NULL, 0, 65535, NULL);

			DWORD threadId;
			sendThread = ::CreateThread(NULL, 0, sendingThreadFunc, this, 0, &threadId);
			recvThread = ::CreateThread(NULL, 0, receivingThreadFunc, this, 0, &threadId);
			
			return true;
		}

		return false;
	}

	void Communicator::close() {
		::SetEvent(closeEvent);
		stream->close();

		// 等待数据发送线程‘寿终正寝’
		::WaitForSingleObject(sendThread, INFINITE);
		::WaitForSingleObject(recvThread, INFINITE);

		::CloseHandle(departuresSemaphore);
	}

	pair<const uint8_t*,size_t> Communicator::buildPayload(const Departure& departure, uint32_t seqId) {
		uint8_t buffer[1024] = {0};

		StreamWriter writer(buffer);
		writer<<departure.commandId()<<seqId;
		departure.serialize(writer);

		size_t payloadSize = writer.bytesWritten();

		uint8_t* payload = new uint8_t[payloadSize];
		memcpy_s(payload, payloadSize, buffer, payloadSize);

		return make_pair(payload, payloadSize);
	}

	void Communicator::exchange(const Departure* departure, Arrival* arrival, ResponseAction action) {
		uint32_t departureId;
		exchangeLock->lock([this, &departureId, departure, arrival, &action](){
			departureId = sequenceId++;
			seqid2arrival->insert(make_pair(departureId, make_pair(arrival, action)));
			departures->push_back(make_tuple(departureId, departure, [](bool, const string&){}));
		});

		::ReleaseSemaphore(departuresSemaphore, 1, NULL);
	}

	void Communicator::registerPassiveArrivalHandler(uint32_t cmdId, GivenArrivalHandler handler) {
		givenHandlers->insert(make_pair(cmdId, handler));
	}
	
	void Communicator::handleSending() {
		bool moreJobs = true;
		while (moreJobs) {
			HANDLE events[] = { closeEvent, departuresSemaphore };
			DWORD eventIdx = ::WaitForMultipleObjects(sizeof(events)/sizeof(events[0]), events, FALSE, INFINITE);
			eventIdx -= WAIT_OBJECT_0;
			if (eventIdx==0) {
				moreJobs = false;
			} else {
				tuple<uint32_t, const Departure*, ResponseAction> departTuple;
				exchangeLock->lock([this, &departTuple]{
					// 既然信号量被释放，意味着departures中一定有待发送的Departure
					departTuple = departures->front();
					departures->pop_front();
				});
				uint32_t departId = get<0>(departTuple);
				const Departure* departure = get<1>(departTuple);
				ResponseAction departureAction = get<2>(departTuple);

				auto payloadPair = buildPayload(*departure, departId);
				const uint8_t* payload = payloadPair.first;
				size_t payloadSize = payloadPair.second;

				stream->send(payload, payloadSize);

				delete payload;
				departureAction(true, "");
			}
		}
	}

	void Communicator::handleReceiving() {
		bool success = true;
		while(success) {
			success = stream->recv([this](const uint8_t* payload, size_t payloadSize){
				printf("income package, size:%d\n", payloadSize);
				for (size_t i=0; i<payloadSize; ++i) {
					printf("%02X ", payload[i]);
					if ( ((i+1)%16)==0 ) {
						printf("\n");
					}
				}
				printf( payloadSize%16==0 ? "\n" : "\n\n");

				StreamReader reader(payload);

				uint32_t commandId, arrivalId;
				reader>>commandId>>arrivalId;

				bool isPassiveArrival = givenHandlers->find(commandId)!=givenHandlers->end();
				if (isPassiveArrival) {
					GivenArrivalHandler& handler = givenHandlers->at(commandId);
					auto counterParts = handler();
					Arrival* specialResponse = get<0>(counterParts);
					const Departure* departure = get<1>(counterParts);
					ResponseAction& arrivalAction = get<2>(counterParts);
					ResponseAction& departureAction = get<3>(counterParts);

					specialResponse->deserialize(reader);
					arrivalAction(true, "");

					exchangeLock->lock([this, arrivalId, departure, &departureAction](){
						departures->push_back(make_tuple(arrivalId, departure, departureAction));
					});
					::ReleaseSemaphore(departuresSemaphore, 1, NULL);
				} else {
					pair<Arrival*, ResponseAction> responsePair;
					exchangeLock->lock([this, &responsePair, arrivalId](){
						responsePair = seqid2arrival->at(arrivalId);
						seqid2arrival->erase(arrivalId);
					});

					Arrival* arrival = responsePair.first;
					ResponseAction action = responsePair.second;
					arrival->deserialize(reader);
					action(true, "");
				}			
			});
		}
	}

	DWORD Communicator::sendingThreadFunc(LPVOID self) {
		((Communicator*)self)->handleSending();
		return 0;
	}

	DWORD Communicator::receivingThreadFunc(LPVOID self) {
		((Communicator*)self)->handleReceiving();
		return 0;
	}
}