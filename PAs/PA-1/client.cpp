/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Sowmith Bonda
	UIN: 434003125
	Date: 09/25/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <fstream> 
#include <iostream> 
#include <vector>
#include <sys/wait.h>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1.0;
	int e = -1;
	int m1 = MAX_MESSAGE;
	bool new_chan_request = false;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m1 = atoi (optarg);
				break;
			case 'c':
				new_chan_request = true;
				break;

		}
	}

	
	std::vector<FIFORequestChannel*> channels;
	int pid = fork();

	if (pid == 0) {
		//child process - run server
		//pass m to server
		std::string m_str = std::to_string(m1);
		execl("./server", "server", "-m", m_str.c_str(), (char*)nullptr);
		perror("exec failed");
    	_exit(127);

	}

	if (pid > 0) {
		//parent process - run client
		FIFORequestChannel chan1("control", FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(&chan1);

		if(new_chan_request) {
			MESSAGE_TYPE nc = NEWCHANNEL_MSG;
			chan1.cwrite(&nc, sizeof(MESSAGE_TYPE));
			char namebuf[64] = {0};                  // size just needs to cover the server's name
			chan1.cread(namebuf, sizeof(namebuf));   // read the c-string
			std::string ncName(namebuf);
			FIFORequestChannel* p = new FIFORequestChannel(ncName.c_str(), FIFORequestChannel::CLIENT_SIDE);
			channels.push_back(p);
		}
		
		FIFORequestChannel* chan = channels.back();
		if(p != -1 && e != -1 && t != -1.0) {
			datamsg push(p, t, e);
			char buf[MAX_MESSAGE];
			memcpy(buf, &push, sizeof(datamsg));
			chan->cwrite(buf, sizeof(datamsg));

			double reply = 0.0;
			chan->cread(&reply, sizeof(double));
			cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
		}else if(p != -1) {
			double time = 0.0;
			ofstream outputFile("received/x1.csv");
			for(int i = 0; i < 1000; i++) {
				datamsg push1(p, time, 1);
				char buf[MAX_MESSAGE];
				memcpy(buf, &push1, sizeof(datamsg));
				chan->cwrite(buf, sizeof(datamsg));

				double reply1 = 0.0;
				chan->cread(&reply1, sizeof(double));

				datamsg push2(p, time, 2);
				// char buf[MAX_MESSAGE];
				memcpy(buf, &push2, sizeof(datamsg));
				chan->cwrite(buf, sizeof(datamsg));

				double reply2 = 0.0;
				chan->cread(&reply2, sizeof(double));

				outputFile << time << ',' << reply1 << ',' << reply2 << endl;
				time += 0.004;
			}
			outputFile.close();
		}

		if(!filename.empty()){
			filemsg fm(0, 0);
			int len = sizeof(filemsg) + (filename.size() + 1);
			char* buf3 = new char[len];

			memcpy(buf3, &fm, sizeof(filemsg));
			strcpy(buf3 + sizeof(filemsg), filename.c_str());
			chan->cwrite(buf3, len);

			__int64_t filesize = 0;
			chan->cread(&filesize, sizeof(__int64_t));

			ofstream outputFile("received/"+filename, ios::binary);

			char* buf4 = new char[m1]; //read
			char* buf5 = new char[len]; //write
			__int64_t offset = 0;

			while(offset < filesize - m1) {
				filemsg fn(offset, m1);
				memcpy(buf5, &fn, sizeof(filemsg));
				strcpy(buf5 + sizeof(filemsg), filename.c_str());
				chan->cwrite(buf5, len);

				chan->cread(buf4, m1);
				outputFile.write(buf4, m1);
				offset+=m1;
			}

			__int64_t lastChunk = filesize - offset;
			filemsg fn(offset, lastChunk);
			memcpy(buf5, &fn, sizeof(filemsg));
			strcpy(buf5 + sizeof(filemsg), filename.c_str());
			chan->cwrite(buf5, len);

			chan->cread(buf4, lastChunk);
			outputFile.write(buf4, lastChunk);
			outputFile.close();
			delete[] buf5;
			delete[] buf4;
			delete[] buf3;
		}

		// closing the channels
		for(long unsigned int i = 0; i < channels.size(); i++) {    
			MESSAGE_TYPE m = QUIT_MSG;
			FIFORequestChannel* chan = channels[i];
			chan->cwrite(&m, sizeof(MESSAGE_TYPE));
			if(i != 0){
				delete chan;
			}
		}

		int status = 0;
		waitpid(pid, &status, 0);
		return 0;
	}

	
	

}
