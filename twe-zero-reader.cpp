// Compile by: clang++ -o twe-zero-reader twe-zero-reader.cpp -std=c++11
#include <cstdio>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <iostream>
#include <signal.h>
#include <iomanip>

using namespace std;

#define TCGETS 0x5401
#define TCSETS 0x5402

int main(int argc, char **argv)
{
    std::ios_base::sync_with_stdio(false);

    if (argc < 2) {
        cerr << "USAGE: twe-zero-reader /dev/tty.usbserial-XXXXXX" << endl;
        return 1;
    }

    int fd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        perror(argv[1]);
        return 2;
    }

    struct sigaction saio;
    saio.sa_handler = [](int) {};
    saio.sa_mask = 0;
    saio.sa_flags = 0;
    sigaction(SIGIO, &saio, nullptr);

    struct termios oldtio, newtio;
    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        return 2;
    }
    newtio = oldtio;
    newtio.c_cflag = CRTSCTS  | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = ICRNL;
    newtio.c_oflag = 0;
    newtio.c_lflag = ICANON;
    cfsetspeed(&newtio, 115200);
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return 2;
    }

    cout << "Now reading values..." << endl;

    char buf[256];
    while (1) {
        usleep(100000);

        while (1) {
            int count = read(fd, buf, 255);
            if (count > 0) {
                buf[count] = 0;
                if (buf[0] == ':') {
                    
                    unsigned char bits[24];
                    auto decodeHex = [](char c) {
                        if (c >= '0' && c <= '9') {
                            return c - '0';
                        } else if (c >= 'A' && c <= 'F') {
                            return c - 'A' + 10;
                        } else {
                            return 0;
                        }
                    };
                    for (int i = 0; i < 24; ++i) {
                        if (i * 2 + 2 < count) {
                            bits[i] = decodeHex(buf[i * 2 + 1]) * 16 + decodeHex(buf[i * 2 + 2]);
                        } else {
                            bits[i] = 0;
                        }
                    }

                    int logicalId = bits[0];
                    int type = bits[1];

                    cout << "LID=" << hex << setw(2) << setfill('0') << logicalId << " ";

                    if (type == 0x81) {
                        int packetId = bits[2];
                        int protocol = bits[3];
                        int lqi = bits[4];
                        int id = bits[8] | (bits[7] << 8) | (bits[6] << 16) | (bits[5] << 24);
                        int toLogicalId = bits[9];

                        cout << "UPDATE #" << packetId << " proto=" << protocol << " lqi=" << lqi;
                        cout << " (" << dec << setw(4) << setfill(' ') << (7 * lqi - 1970) / 20 << "dBm) ";
                        //cout;
                        cout << "from " << hex << setfill('0') << setw(8) << id << " to " << toLogicalId;
                        cout << endl;

                        int timestamp = (bits[10] << 8) | bits[11];
                        int relay = bits[12];
                        int volts = (bits[13] << 8) | bits[14];
                        int unused = bits[15];

                        cout << "ts=" << dec << setw(5) << setfill(' ') << timestamp << " (" <<
                        setw(8) << setfill(' ') << (timestamp * 1000 / 64) << "/4194304ms) ";
                        cout << "vcc=" << dec << setw(5) << setfill(' ') << volts << "mV" ;
                        cout << endl;

                        int digitalIn = bits[16];
                        int digitalInUpd = bits[17];
                        int analog[4];
                        analog[0] = bits[21] * 16 + ((bits[22] >> 6) & 3) * 4;
                        analog[1] = bits[20] * 16 + ((bits[22] >> 4) & 3) * 4;
                        analog[2] = bits[19] * 16 + ((bits[22] >> 2) & 3) * 4;
                        analog[3] = bits[18] * 16 + ((bits[22] >> 0) & 3) * 4;
                        cout << "Digital: "; 
                        for (int i = 0; i < 4; ++i) {
                            if ((digitalIn >> i) & 1)
                                cout << " [ DI" << i+1 << " ] ";
                            else
                                cout << " ----" <<        "-- ";
                        }
                        cout << endl;
                        cout << "Analog:" << endl;
                        for (int i = 0; i < 4; ++i) {
                            cout << "AI" << i+1 << ": ";
                            if (bits[21 - i] == 255) {
                                cout << "----mV" << endl;
                            } else {
                                cout << setw(4) << analog[i] << "mV [";
                                for (int k = 0; k < 2000; k += 50) {
                                    cout << (k <= analog[i] ? '*' : ' ');
                                }
                                cout << "]" << endl;
                            }
                        }

                        int checksum = bits[23];
                        // FIXME: checksum


                    } else {
                        cout << "??: " << (buf + 1);
                    }

                    cout << "-------------------------------------------------" << endl;

                } else if (buf[0] == '!') {
                    if (buf[count - 1] == '\n' || buf[count - 1] == '\r')
                        buf[count - 1] = 0;
                    cout << "MSG: " << (buf + 1) << endl;;
                } else if (count > 1) {
                    cout << "[ malformed: " << buf << "]" << endl;;
                }
            } else {
                break;
            }
        }
    }
}