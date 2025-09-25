/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name:
	UIN:
	Date:
*/
#include "common.h"
#include "FIFORequestChannel.h"

#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <fstream>

using namespace std;

static void ensure_received_dir() {
	struct stat st;
	if (stat("received", &st) == -1) {
		if (mkdir("received", 0777) == -1 && errno != EEXIST) {
			EXITONERROR("mkdir(received)");
		}
	}
}

int main (int argc, char *argv[]) {
	// ---------------------------
	// Parse command-line options
	// ---------------------------
	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	string filename = "";
	int m = MAX_MESSAGE;         // buffer capacity (client-side)
	bool want_new_channel = false;

	bool p_set = false;
	bool t_set = false;
	bool e_set = false;
	bool f_set = false;

	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi(optarg);
				p_set = true;
				break;
			case 't':
				t = atof(optarg);
				t_set = true;
				break;
			case 'e':
				e = atoi(optarg);
				e_set = true;
				break;
			case 'f':
				filename = optarg;
				f_set = true;
				break;
			case 'm':
				m = atoi(optarg);
				if (m <= 0) m = MAX_MESSAGE;
				break;
			case 'c':
				want_new_channel = true;
				break;
		}
	}

	// -------------------------------------------------
	// Fork & exec the server as a child of this client
	// -------------------------------------------------
	pid_t pid = fork();
	if (pid < 0) {
		EXITONERROR("fork");
	}

	if (pid == 0) {
		// -----------------
		// Child: run server
		// -----------------
		if (m == MAX_MESSAGE) {
			execl("./server", "server", (char*)nullptr);
		} else {
			std::string m_str = std::to_string(m);
			execl("./server", "server", "-m", m_str.c_str(), (char*)nullptr);
		}
		// Only reached if exec fails
		perror("execl server failed");
		_exit(127);
	}

	// ---------------------------
	// Parent: connect to control
  	// ---------------------------
	FIFORequestChannel* control = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);

	// If requested, create/join a new data channel
	FIFORequestChannel* data_chan = control; // default to control channel
	if (want_new_channel) {
		MESSAGE_TYPE nm = NEWCHANNEL_MSG;
		control->cwrite(&nm, sizeof(MESSAGE_TYPE)); // request new channel

		// server responds with a c-string channel name
		char namebuf[64] = {0};
		int n = control->cread(namebuf, sizeof(namebuf));
		if (n <= 0) {
			cerr << "Failed to receive new channel name" << endl;
			// We'll continue using control channel if creation failed
		} else {
			data_chan = new FIFORequestChannel(string(namebuf), FIFORequestChannel::CLIENT_SIDE);
		}
	}

	// Helper lambda to close a channel (send QUIT then delete)
	auto close_channel = [](FIFORequestChannel* ch){
		if (!ch) return;
		MESSAGE_TYPE q = QUIT_MSG;
		ch->cwrite(&q, sizeof(MESSAGE_TYPE));
		delete ch;
	};

	// ------------------------------------------------
	// Mode selection:
	//   1) If -f given: file transfer
	//   2) Else if -t (single data point): request it
	//   3) Else if only -p: first 1000 datapoints
	// ------------------------------------------------
	if (f_set) {
		// --------------------------
		// 4.3 File Request (binary)
		// --------------------------
		ensure_received_dir();

		// First, request file size via filemsg(0,0) + "name\0" in ONE write
		filemsg fm0(0, 0);
		int req_len0 = sizeof(filemsg) + (filename.size() + 1);
		char* req0 = new char[req_len0];
		memcpy(req0, &fm0, sizeof(filemsg));
		strcpy(req0 + sizeof(filemsg), filename.c_str());

		data_chan->cwrite(req0, req_len0); // ask for file size
		delete[] req0;

		__int64_t filesize = 0;
		int n = data_chan->cread(&filesize, sizeof(__int64_t));
		if (n != (int)sizeof(__int64_t)) {
			cerr << "Failed to read file size\n";
		} else {
			// Prepare output file under received/
			string outpath = "received/" + filename;
			// Ensure parent directories under received/ (we only guarantee "received")
			// If filename contains subdirs, user should ensure they exist.
			FILE* out = fopen(outpath.c_str(), "wb");
			if (!out) {
				EXITONERROR("fopen(received/<file>)");
			}

			// Transfer in chunks up to m bytes
			__int64_t offset = 0;
			while (offset < filesize) {
				int chunk = (int) std::min((__int64_t)m, filesize - offset);

				filemsg fmsg(offset, chunk);
				int req_len = sizeof(filemsg) + (filename.size() + 1);
				char* req = new char[req_len];
				memcpy(req, &fmsg, sizeof(filemsg));
				strcpy(req + sizeof(filemsg), filename.c_str());

				data_chan->cwrite(req, req_len); // single write for header+name
				delete[] req;

				// Read data
				char* rbuf = new char[chunk];
				int got = data_chan->cread(rbuf, chunk);
				if (got != chunk) {
					cerr << "Short read while receiving file chunk\n";
					delete[] rbuf;
					break;
				}
				// Write at correct position
				size_t w = fwrite(rbuf, 1, chunk, out);
				(void)w;
				delete[] rbuf;
				offset += chunk;
			}
			fclose(out);
			// Optional: print confirmation
			// cout << "Received file \"" << filename << "\" (" << filesize << " bytes) into " << outpath << endl;
		}
	}
	else if (t_set) {
		// ---------------------------------------
		// 4.2 Single Data Point: -p, -t, -e
		// ---------------------------------------
		// Construct datamsg and send
		datamsg req(p, t, e);
		data_chan->cwrite(&req, sizeof(req));

		double reply = 0.0;
		int nr = data_chan->cread(&reply, sizeof(double));
		if (nr != (int)sizeof(double)) {
			cerr << "Failed to read data point\n";
		} else {
			cout << "For person " << p << ", at time " << t
				 << ", the value of ecg " << e << " is " << reply << endl;
		}
	}
	else if (p_set && !f_set) {
		// -----------------------------------------------------------
		// 4.2 First 1000 data rows (time, ecg1, ecg2) for patient p
		// Writes to x<p>.csv and (optionally) can be diff'ed by user
		// -----------------------------------------------------------
		const int N = 1000;
		string outname = "x" + to_string(p) + ".csv";
		ofstream ofs(outname);
		if (!ofs.good()) {
			EXITONERROR("ofstream x<p>.csv");
		}
		ofs << "time,ecg1,ecg2\n";

		for (int i = 0; i < N; i++) {
			double ti = i * 0.004; // 4ms per sample
			// ecg1
			datamsg r1(p, ti, 1);
			data_chan->cwrite(&r1, sizeof(r1));
			double v1 = 0.0;
			int n1 = data_chan->cread(&v1, sizeof(double));
			if (n1 != (int)sizeof(double)) {
				cerr << "Failed to read ecg1 at i=" << i << endl;
				break;
			}
			// ecg2
			datamsg r2(p, ti, 2);
			data_chan->cwrite(&r2, sizeof(r2));
			double v2 = 0.0;
			int n2 = data_chan->cread(&v2, sizeof(double));
			if (n2 != (int)sizeof(double)) {
				cerr << "Failed to read ecg2 at i=" << i << endl;
				break;
			}
			ofs << fixed << ti << "," << v1 << "," << v2 << "\n";
		}
		ofs.close();
		// User can compare with "head -n 1001 BIMDC/<p>.csv" or similar.
	}

	// ------------------------------------
	// 4.5 Close channels & reap the child
	// ------------------------------------
	if (data_chan != control) {
		close_channel(data_chan);   // new data channel, if created
	}
	close_channel(control);         // control channel

	int status = 0;
	waitpid(pid, &status, 0);       // wait for server to exit

	return 0;
}
