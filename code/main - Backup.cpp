#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <list>
#include <deque>
#include <stdlib.h>
#include <string.h>

using namespace std;

long PC, mem_AD;
int op_Type, reg_D, reg_S1, reg_S2;
int numInst = 0, numCycle = 0, numCompletedInst=0, numRetire=0, bonusPart=1;
int cycleNo = 0, seqNo = 0, issuedInst = 0, execQSize = 0, mode;
int flagIF=0, flagID=0, flagIS1=0, flagIS2=0, flagWB=0;
int S=0, N=0, l1Size=0, l1Assoc=0, l2Size=0, l2Assoc=0, blockSize=0;
int l1Result=0, l2Result=0;
ifstream file;

enum state { IF, ID, IS, EX, WB };
class ROB
{
	public:
		long mem_AD;
		int op_Type, op_Latency, tag, valid;
        int reg_D, reg_S1, reg_S2, tag_S1, tag_S2, tag_D;
        int ready_D, ready_S1, ready_S2;
        int IF_BeginCycle, ID_BeginCycle, IS_BeginCycle, EX_BeginCycle, WB_BeginCycle;
        int IF_Duration, ID_Duration, IS_Duration, EX_Duration, WB_Duration;
		state state;

	ROB() {
		state = IF;
		valid = 1;
		tag_S1 = 0;
		tag_S2 = 0;
		tag_D = 0;
		IF_BeginCycle = 0;
		ID_BeginCycle = 0;
		IS_BeginCycle = 0;
		EX_BeginCycle = 0;
		WB_BeginCycle = 0;
		IF_Duration = 0;
		ID_Duration = 0;
		IS_Duration = 0;
		EX_Duration = 0;
		WB_Duration = 0;
	};
};

vector<ROB> fakeROB;
deque<ROB> dispQ, dispL, schdQ, execQ;
list<ROB> finalQ;
list<ROB>::iterator it = finalQ.begin();

class regfile
{
	public: 
		int ready;
		int tag;
	regfile() {
		ready = 1;
		tag = 0;
	};
}registers[256];

void printStats();
void initFakeROB();
bool programOrder(ROB l1, ROB l2);

bool AdvanceCycle();
void FakeRetire();
void Execute();
void Issue();
void Dispatch();
void Fetch();

class Cache{
    public: char tag[8];
    public: int validFlag;
    public: int dirty;
    public: int recentUsed;
}cacheL1[1024][64], cacheL2[4096][128];

int L1CRead=0, L1CReadMiss=0, L2CRead=0, L2CReadMiss=0;
int l1SetCount=0, l2SetCount=0, recent, associated;
int blockOffset=0, indexLength1=0, indexLength2=0, tagLength1=0, tagLength2=0;
char binaryAddress[32], hexaAddress[8];
const int binLength = 32;

void initializeL1Cache(int assoc, int setCount);
void initializeL2Cache(int assoc, int setCount);
void displayL1Cache();
void displayL2Cache();
int readCacheL1(char address[], int setCount, int assoc);
int readCacheL2(char address[], int setCount, int assoc);
void updateLRU1(int newSet, int newCol, int assoc, int miss, int recent);
void updateLRU2(int newSet, int newCol, int assoc, int miss, int recent);
int getNumberOfBits(int size);
int getIndexValue(char bin[], int indexLength, int blockOffset);
int getBlockValue(char bin[], int blockOffset);
int getTagLength(int blockOffset, int indexLength, int size);
void getTagFromBinaryAddress(char bin[], int indexLength, int blockOffset);
void convertToBinary(char hexa[]);
void convertToHexa(char bin[], int limit);

int main(int argc, char *argv[]) 
{
	S = atoi(argv[1]);
	N = atoi(argv[2]);
	blockSize = atoi(argv[3]);
 	l1Size = atoi(argv[4]);
	l1Assoc = atoi(argv[5]);
	l2Size = atoi(argv[6]);
	l2Assoc = atoi(argv[7]);
	file.open(argv[8]);
	if (bonusPart==1) {
		if (l1Size!=0 && l1Assoc!=0 && blockSize!=0) {
			blockOffset = getNumberOfBits(blockSize);
			l1SetCount = l1Size/(l1Assoc*blockSize);
			indexLength1 = getNumberOfBits(l1SetCount);
			initializeL1Cache(l1Assoc, l1SetCount);
			if (l2Size!=0 && l2Assoc!=0) {
				l2SetCount = l2Size/(l2Assoc*blockSize);
				indexLength2 = getNumberOfBits(l2SetCount);
				initializeL2Cache(l2Assoc, l2SetCount);
			}
		}
	}

	while (file>>hex>>PC>>dec>>op_Type>>dec>>reg_D>>dec>>reg_S1>>dec>>reg_S2>>hex>>mem_AD) {
		numInst++;
	}
	file.close();
	file.clear();

	file.open(argv[8]);
	initFakeROB();

	do{
		FakeRetire();
		Execute();
		Issue();
		Dispatch();
		Fetch();
	} while (AdvanceCycle());

	file.close();
	printStats();
	return 0;
}

void initFakeROB() {
	ROB* ffakeROB = new ROB();
	int i=0;
	while (file>>hex>>PC>>dec>>op_Type>>dec>>reg_D>>dec>>reg_S1>>dec>>reg_S2>>hex>>mem_AD) {
		ffakeROB->tag = i;
		ffakeROB->op_Type = op_Type;
        ffakeROB->reg_S1 = reg_S1;
        ffakeROB->ready_S1 = (reg_S1 == -1) ? -1 : ffakeROB->ready_S1;
        ffakeROB->reg_S2 = reg_S2;
        ffakeROB->ready_S2 = (reg_S2 == -1) ? -1 : ffakeROB->ready_S2;
        ffakeROB->reg_D = reg_D;
        ffakeROB->mem_AD = mem_AD;
        switch(op_Type) {
            case 0: ffakeROB->op_Latency=1; break;
            case 1: ffakeROB->op_Latency=2; break;
            case 2: ffakeROB->op_Latency=5; break;
            default: ffakeROB->op_Latency=1;
        }
		i++;
		fakeROB.push_back(*ffakeROB);
	}
}

bool AdvanceCycle() {
	cycleNo++;
	return (numInst!=numRetire);
}

void FakeRetire() {
	int i;
	for (i=0; i<numInst; i++) {
		if (fakeROB[i].state==WB && fakeROB[i].valid!=0) {
			fakeROB[i].valid = 0;
			numRetire++;
		}
	}
}

void Execute() {
	int i,j,k;
    execQSize = execQ.size();
	for (k=0; k<execQSize; k++) {
		for (i=0; i<execQ.size(); i++) {
			if ((execQ[i].state == EX) && (execQ[i].op_Latency == 0)) {
   				execQ[i].state = WB;
  				execQ[i].WB_BeginCycle = cycleNo;
				execQ[i].WB_Duration++;
				advance(it, i);
				finalQ.insert(it,execQ[i]);
				fakeROB[execQ[i].tag].state = execQ[i].state;

				//Update register value
				if (registers[execQ[i].reg_D].tag == execQ[i].tag) {
					registers[execQ[i].reg_D].tag = execQ[i].tag_D;
					registers[execQ[i].reg_D].ready = 1;
				}
				//Wakeup the dependent instructiosn
				for (j=0; j<schdQ.size(); j++) {
					if ((schdQ[j].tag_S1 == execQ[i].tag) && (schdQ[j].ready_S1 == 0)) {
						schdQ[j].tag_S1 = execQ[i].tag_S1;
						schdQ[j].ready_S1 = 1;
					}
					if ((schdQ[j].tag_S2 == execQ[i].tag) && (schdQ[j].ready_S2 == 0)) {
						schdQ[j].tag_S2 = execQ[i].tag_S2;
						schdQ[j].ready_S2 = 1;
					}
				}
				execQ.erase(execQ.begin() + i);
				break;
			}
		}
	}
	for (i=0; i<execQ.size(); i++) {
		execQ[i].op_Latency--;
		execQ[i].EX_Duration++;
	}
}

void Issue() {
	int i,j,k;
	issuedInst = 0;
	char address[32];
	for (k=0; k<S; k++) {
		if (issuedInst < N) {
			for (i=0; i<schdQ.size(); i++) {
				if (((schdQ[i].ready_S1 != 0) && (schdQ[i].ready_S2 != 0))) {
					issuedInst++;
					schdQ[i].state = EX;
                    schdQ[i].EX_BeginCycle = cycleNo;
					if (bonusPart == 1) 
					{
					if (l1Size!=0 && l1Assoc!=0) {
						if ((schdQ[i].op_Type==2) && (schdQ[i].mem_AD!=0))
						{
							sprintf(address, "%X", schdQ[i].mem_AD);
							l1Result = readCacheL1(address, l1SetCount, l1Assoc);
							if (l1Result == 1) {
								schdQ[i].op_Latency = 5;
								fakeROB[schdQ[i].tag].op_Latency = 5;
							}
							else if (l1Result > 1) {
								if (l2Size==0 || l2Assoc==0) {
									schdQ[i].op_Latency = 20;
									fakeROB[schdQ[i].tag].op_Latency = 20;
								}
								else {
									sprintf(address, "%X", schdQ[i].mem_AD);
									l2Result = readCacheL2(address, l2SetCount, l2Assoc);
									if (l2Result == 1) {
										schdQ[i].op_Latency = 10;
										fakeROB[schdQ[i].tag].op_Latency = 10;
									}
									else if (l2Result > 1) {
										schdQ[i].op_Latency = 20;
										fakeROB[schdQ[i].tag].op_Latency = 20;
									}
								}
							}
						}
					}
					}
                    schdQ[i].EX_Duration++;
					schdQ[i].op_Latency--;
					execQ.push_back(schdQ[i]);
					schdQ.erase(schdQ.begin() + i);
					break;
				}
			}
		}
		if (issuedInst == N) {
			for (j=0; j<schdQ.size(); j++) {
				schdQ[j].IS_Duration++;
			}
			break;
		}
	}
	if (issuedInst < N)  {
		for (j=0; j<schdQ.size(); j++) {
			schdQ[j].IS_Duration++;
		}
	}
}

void Dispatch() {
	int i,k;
	//For instructions in ID stage
	for (k=0; k<S; k++) {
		if (dispL.size() != 0 && flagID==0) {
			//If schedule Q full, simply stall
			if (schdQ.size() == S) {
				for (i=0; i<dispL.size(); i++) {
					dispL[i].ID_Duration++;
				}
				flagID=1;
			}
			//Process dispatch list only if ready for IS stage
			else if (flagID==0) {
				dispL.front().state = IS;
        		dispL.front().IS_BeginCycle = cycleNo;
				dispL.front().IS_Duration++;
				schdQ.push_back(dispL.front());
				dispL.pop_front();
				//Rename source operand 1 by using RF
				if (schdQ.back().reg_S1 != -1) {
					schdQ.back().tag_S1 = registers[schdQ.back().reg_S1].tag;
					schdQ.back().ready_S1 = registers[schdQ.back().reg_S1].ready;
				}
				//Rename source operand 2 by using RF
				if (schdQ.back().reg_S2 != -1) {
					schdQ.back().tag_S2 = registers[schdQ.back().reg_S2].tag;
					schdQ.back().ready_S2 = registers[schdQ.back().reg_S2].ready;
				}
				//Rename destination operand by updating RF
				if (schdQ.back().reg_D != -1) {
					registers[schdQ.back().reg_D].ready = 0;
					registers[schdQ.back().reg_D].tag = schdQ.back().tag;
					schdQ.back().tag_D = registers[schdQ.back().reg_D].tag;
				}
			}
		}
	}
	flagID=0;
	//For instructions in IF stage
	for (k=0; k<N; k++) {
		if (dispQ.size() != 0 && flagIF==0) {
			//Implement 1 cycle delay if in IF stage
			if (dispL.size() == N) {
				for (i=0; i<dispQ.size(); i++) {
					dispQ[i].ID_Duration++;
				}
				flagIF=1;
			}
			//Decode instruction only if ready for ID stage
			else if (flagIF==0) {
				dispQ.front().state = ID;
				dispQ.front().ID_BeginCycle = dispQ.front().IF_BeginCycle + 1;
				dispQ.front().ID_Duration++;
				dispL.push_back(dispQ.front());
				dispQ.pop_front();
			}
		}
	}
	flagIF=0;
}

void Fetch() {
	int i;
	for (i=0; i<N; i++) {
		//Process instruction only if //1. Not end of file //2. Dispatch Q not full
		if ((seqNo != numInst) && (dispQ.size() != N))	{
			fakeROB[seqNo].state = IF;
			fakeROB[seqNo].IF_BeginCycle = cycleNo;
			fakeROB[seqNo].IF_Duration++;
			dispQ.push_back(fakeROB[seqNo]);
			seqNo++;
		}
	}
}

void printStats() {
	list<ROB>::iterator i;
	finalQ.sort(programOrder);
	for (i= finalQ.begin(); i!=finalQ.end(); i++) {
		ROB rob = *i;
		cout<<rob.tag<<" "<<"fu{"<<rob.op_Type<<"}"
            <<" src{"<<rob.reg_S1<<","<<rob.reg_S2<<"}"
		    <<" dst{"<<rob.reg_D<<"} "
            <<" IF{"<<rob.IF_BeginCycle<<","<<rob.IF_Duration<<"}"
		    <<" ID{"<<rob.ID_BeginCycle<<","<<rob.ID_Duration<<"}"
		    <<" IS{"<<rob.IS_BeginCycle<<","<<rob.IS_Duration<<"}"
		    <<" EX{"<<rob.EX_BeginCycle<<","<<rob.EX_Duration<<"}"
		    <<" WB{"<<rob.WB_BeginCycle<<","<<rob.WB_Duration<<"}\n";
	}
	if (bonusPart==1) {
		if (l1Size!=0 && l1Assoc!=0 && blockSize!=0) {
			displayL1Cache();
			cout<<"\n";
			if (l2Size!=0 && l2Assoc!=0) {
				displayL2Cache();
			}
		}
	}
    cout<<"CONFIGURATION"
        <<"\nsuperscalar bandwidth (N) = "<<N
        <<"\ndispatch queue size (2*N) = "<<2*N
        <<"\nschedule queue size (S) = "<<S
        <<"\nRESULTS"
        <<"\nnumber of instructions = "<<numInst
        <<"\nnumber of cycles = "<<cycleNo-1;
	printf("\nIPC = %0.2f\n",((float)numInst)/((float)(cycleNo-1)));
}

bool programOrder(ROB l1, ROB l2) {
	return l1.tag < l2.tag;
}

void initializeL1Cache(int assoc, int setCount) 
{
	int i, j;
    Cache newCache[setCount][assoc];
    for(i=0; i<setCount; i++) {
        for (j=0; j<assoc; j++) {
            newCache[i][j].validFlag = 0;
            strcpy(newCache[i][j].tag, "0");
            newCache[i][j].recentUsed = 9999;
            newCache[i][j].dirty = 0;
            cacheL1[i][j] = newCache[i][j];
        }
    }
}

void initializeL2Cache(int assoc, int setCount) 
{
	int i, j;
    Cache newCache[setCount][assoc];
    for(i=0; i<setCount; i++) {
        for (j=0; j<assoc; j++) {
            newCache[i][j].validFlag = 0;
            strcpy(newCache[i][j].tag, "0");
            newCache[i][j].recentUsed = 9999;
            newCache[i][j].dirty = 0;
            cacheL2[i][j] = newCache[i][j];
        }
    }
}

void displayL1Cache()
{
	int i,j,k,m,n;
    cout<<"L1 CACHE CONTENTS"
		<<"\na. number of accesses :"<<L1CRead
		<<"\nb. number of misses :"<<L1CReadMiss;
    Cache tempCache;
    for(int i=0; i<l1SetCount; i++) {
        cout<<"\nset "<<i<<":";
        for (j=0; j<l1Assoc; j++) {
            cout<<"  "<<cacheL1[i][j].tag;
        }
    }
    cout<<"\n";
}

void displayL2Cache()
{
	int i,j,k,m,n;
    cout<<"L2 CACHE CONTENTS"
		<<"\na. number of accesses :"<<L2CRead
		<<"\nb. number of misses :"<<L2CReadMiss;
    Cache tempCache;
    for(int i=0; i<l2SetCount; i++) {
        cout<<"\nset "<<i<<":";
        for (j=0; j<l2Assoc; j++) {
            cout<<"  "<<cacheL2[i][j].tag;
        }
    }
    cout<<"\n\n";
}

// Reads from cache using WBWA policy
int readCacheL1(char address[], int setCount, int assoc)
{
	int i,j;
    convertToBinary(address);
    getTagFromBinaryAddress(binaryAddress, indexLength1, blockOffset);
    tagLength1 = strlen(hexaAddress);
    int index = getIndexValue(binaryAddress, indexLength1, blockOffset);
    // Operation A -> if cache-hit occurs
    L1CRead++;
    for (j=0; j<assoc; j++) {
        if(cacheL1[index][j].validFlag == 1) {
            if(strncmp(cacheL1[index][j].tag, hexaAddress, tagLength1) == 0) {
                recent = cacheL1[index][j].recentUsed;
                updateLRU1(index, j, assoc, 0, recent);
                return 1;
            }
        }
    }

    // Operation B -> if cache-miss occurs
    L1CReadMiss++;
    for (j=0; j<assoc; j++) {
        if(cacheL1[index][j].validFlag == 0) {
            cacheL1[index][j].dirty = 0;
            cacheL1[index][j].validFlag = 1;
            strcpy(cacheL1[index][j].tag, hexaAddress);
            updateLRU1(index, j, assoc, 1, 0);
            return 2;
        }
    }
    
    recent = 0;
    associated = 0;
    for (j=0; j<assoc; j++) {
        if(cacheL1[index][j].recentUsed > recent) {
            recent = cacheL1[index][j].recentUsed;
            associated = j;
        }
    }
	
    strcpy(cacheL1[index][associated].tag, hexaAddress);
    updateLRU1(index, associated, assoc, 1, 0);
    return 3;
}

// Reads from cache using WBWA policy
int readCacheL2(char address[], int setCount, int assoc)
{
	int i,j;
    convertToBinary(address);
    getTagFromBinaryAddress(binaryAddress, indexLength2, blockOffset);
    tagLength2 = strlen(hexaAddress);
    int index = getIndexValue(binaryAddress, indexLength2, blockOffset);
    // Operation A -> if cache-hit occurs
    L2CRead++;
    for (j=0; j<assoc; j++) {
        if(cacheL2[index][j].validFlag == 1) {
            if(strncmp(cacheL2[index][j].tag, hexaAddress, tagLength2) == 0) {
                recent = cacheL2[index][j].recentUsed;
                updateLRU2(index, j, assoc, 0, recent);
                return 1;
            }
        }
    }

    // Operation B -> if cache-miss occurs
    L2CReadMiss++;
    for (j=0; j<assoc; j++) {
        if(cacheL2[index][j].validFlag == 0) {
            cacheL2[index][j].dirty = 0;
            cacheL2[index][j].validFlag = 1;
            strcpy(cacheL2[index][j].tag, hexaAddress);
            updateLRU2(index, j, assoc, 1, 0);
            return 2;
        }
    }
    
    recent = 0;
    associated = 0;
    for (j=0; j<assoc; j++) {
        if(cacheL2[index][j].recentUsed > recent) {
            recent = cacheL2[index][j].recentUsed;
            associated = j;
        }
    }
	
    strcpy(cacheL2[index][associated].tag, hexaAddress);
    updateLRU2(index, associated, assoc, 1, 0);
    return 3;
}

void updateLRU1(int newSet, int newCol, int assoc, int miss, int recent)
{
	int j;
    for (j=0; j<assoc; j++) 
    {
        if (cacheL1[newSet][j].validFlag == 1) {
            if (miss == 1) {
                cacheL1[newSet][j].recentUsed++;
            }
            else {
                if(cacheL1[newSet][j].recentUsed < recent){
                    cacheL1[newSet][j].recentUsed++;
                }
            }
        }
    }
    cacheL1[newSet][newCol].recentUsed = 0;
}

void updateLRU2(int newSet, int newCol, int assoc, int miss, int recent)
{
	int j;
    for (j=0; j<assoc; j++) 
    {
        if (cacheL2[newSet][j].validFlag == 1) {
            if (miss == 1) {
                cacheL2[newSet][j].recentUsed++;
            }
            else {
                if(cacheL2[newSet][j].recentUsed < recent){
                    cacheL2[newSet][j].recentUsed++;
                }
            }
        }
    }
    cacheL2[newSet][newCol].recentUsed = 0;
}

// Gets the block-offset (or) index of the program
int getNumberOfBits(int size) 
{
	int i;
    int digits = 0;
    for(i=1; i<size; i*=2) {
        digits++;
    }
    return digits;
}

// Gets the index of given set
int getIndexValue(char bin[], int indexLength, int blockOffset)
{
	int i;
    int index = 0, newBinLength = 0;
    if(strlen(bin) < binLength) {
        newBinLength = strlen(bin);
    }
    else {
        newBinLength = binLength;
    }
    int startBit = newBinLength-blockOffset-1;
    for(i=startBit; i>=newBinLength-indexLength-blockOffset; i--) {
        if(bin[i] == '1')
            index += pow(2,(startBit-i));
    }
    return index;
}

// Gets the block of given set
int getBlockValue(char bin[], int blockOffset)
{
	int i;
    int block = 0, newBinLength = 0;
    if(strlen(bin) < binLength) {
        newBinLength = strlen(bin);
    }
    else {
        newBinLength = binLength;
    }
    int startBit = newBinLength-1;
    for(i=startBit; i>=newBinLength-blockOffset; i--) {
        if(bin[i] == '1') 
            block += pow(2,(startBit-i));
    }
    return block;
}

// Forms the hexa tag for an address
void getTagFromBinaryAddress(char bin[], int indexLength, int blockOffset) 
{
	int i;
    char tempStr[32];
    int len = strlen(bin);
    for (i=0; i<len-indexLength-blockOffset; i++) {
        tempStr[i] = bin[i];
    }
    convertToHexa(tempStr, i);
}

void convertToBinary(char hexa[])
{
	int i;
    strcpy(binaryAddress, "");
    int len = strlen(hexa);
    for(i=0; i<len; i++) {
        switch(hexa[i]){
            case '0': strcat(binaryAddress,"0000"); 
                break;
            case '1': strcat(binaryAddress,"0001"); 
                break;
            case '2': strcat(binaryAddress,"0010"); 
                break;
            case '3': strcat(binaryAddress,"0011"); 
                break;
            case '4': strcat(binaryAddress,"0100"); 
                break;
            case '5': strcat(binaryAddress,"0101"); 
                break;
            case '6': strcat(binaryAddress,"0110"); 
                break;
            case '7': strcat(binaryAddress,"0111"); 
                break;
            case '8': strcat(binaryAddress,"1000"); 
                break;
            case '9': strcat(binaryAddress,"1001"); 
                break;
            case 'a': strcat(binaryAddress,"1010"); 
                break;
            case 'b': strcat(binaryAddress,"1011"); 
                break;
            case 'c': strcat(binaryAddress,"1100"); 
                break;
            case 'd': strcat(binaryAddress,"1101"); 
                break;
            case 'e': strcat(binaryAddress,"1110"); 
                break;
            case 'f': strcat(binaryAddress,"1111"); 
                break;
			case 'A': strcat(binaryAddress,"1010"); 
                break;
            case 'B': strcat(binaryAddress,"1011"); 
                break;
            case 'C': strcat(binaryAddress,"1100"); 
                break;
            case 'D': strcat(binaryAddress,"1101"); 
                break;
            case 'E': strcat(binaryAddress,"1110"); 
                break;
            case 'F': strcat(binaryAddress,"1111"); 
                break;
            default: strcat(binaryAddress,""); 
                break;
        }
    }
}

void convertToHexa(char bin[], int limit) 
{
    int sum, hexVal, padDiff=0, i;
    strcpy(hexaAddress, "");
    if (limit%4 != 0) {
        padDiff = 4 - limit%4;
        for (i=0; i<padDiff; i++) {
            bin[limit+i] = '0';
        }
        for(i=limit+padDiff-1; i>=padDiff; i--) {
            bin[i] = bin[i-padDiff];
        }
        for(i=0; i<padDiff; i++) {
            bin[i] = '0';
        }
    }
    for(i=0; i<limit+padDiff; i+=4) {
        sum=0;
        if (bin[i]=='1') sum+=8;
        if (bin[i+1]=='1') sum+=4;
        if (bin[i+2]=='1') sum+=2;
        if (bin[i+3]=='1') sum+=1;        
        hexVal = sum%16;
        if (hexVal > 9) {
            hexVal %=9;
            switch(hexVal) {
                case 1: strcat(hexaAddress, "a"); break;
                case 2: strcat(hexaAddress, "b"); break;
                case 3: strcat(hexaAddress, "c"); break;
                case 4: strcat(hexaAddress, "d"); break;
                case 5: strcat(hexaAddress, "e"); break;
                case 6: strcat(hexaAddress, "f"); break;
                default: strcat(hexaAddress, ""); break;
            };
        }
        else {
            switch(hexVal) {
                case 1: strcat(hexaAddress, "1"); break;
                case 2: strcat(hexaAddress, "2"); break;
                case 3: strcat(hexaAddress, "3"); break;
                case 4: strcat(hexaAddress, "4"); break;
                case 5: strcat(hexaAddress, "5"); break;
                case 6: strcat(hexaAddress, "6"); break;
                case 7: strcat(hexaAddress, "7"); break;
                case 8: strcat(hexaAddress, "8"); break;
                case 9: strcat(hexaAddress, "9"); break;
                case 0: strcat(hexaAddress, "0"); break;
                default: strcat(hexaAddress, ""); break;
            };
        }
        if (hexaAddress[0]=='0'){
            strcpy(hexaAddress,"");
        }
    }
}