
#include "VirtualMachine.h"
#include "Machine.h"
#include "stdlib.h"
#include "vector"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <list>
#include <fcntl.h> // O_RDWR
#include <queue>

using namespace std;

//@********************************************* Project 4 ************************************
//@*********1. hello.so     OK
//@*********2. sleep.so     OK
//@*********3. file.so      OK
//@*********4. file2.so     Ok
//@*********5. mutex.so     like proj2
//@*********6. thread.so    OK
//@*********7. preempt.so   like proj2
//@*********8. memory.so    OK
//@*********9. shell.so
//@*********10. shell2.so
//@*********11. copyfile.so
//@*********************************************************************************************


extern "C"
{
    TVMMainEntry VMLoadModule(const char *module);
    void VMUnloadModule(void);
    extern void VMStringCopy(char *dest, const char *src);
    extern void VMStringCopyN(char *dest, const char *src, int32_t n);
    
    //************************* Thread Control Block **********************************
    
    typedef struct TCB  //Thread Control Block, maintain information about the process
    {
        int returnFile;                     //use when doing multithread
        int FDresult;
        int returnMutexIndex;
        void *entry;                         //for thread entry parameter used in ThreadcontectCreate
        uint8_t *base;                      //byte size type pointer for base of stack
        TVMThreadID ThreadID;               //hold thread iDs
        TVMThreadState ThreadState;         //dead, running, ready or waiting
        TVMThreadPriority tPriority;        //thread schedular schedules threads according to priority (preemptive scheduling)
        TVMThreadEntry ThreadEntry;
        TVMMemorySize memorySize;
        SMachineContext context;		    //use to switch to/from the thread
        TVMTick SleepTick;
    }TCB;
    
    
    TCB* currentThread = new TCB; //global current thread
    TCB* idleThread = new TCB;    //global idle thread
    vector<TCB*> ThreadsVector;   //global threads vector
    
    //ready threads have 3 prioity levels
    vector<TCB*> highPriority;
    vector<TCB*> lowPriority;
    vector<TCB*> normalPriority;
    
    //scheduling queues
    vector<TCB*> readyQueue;
    vector<TCB*> sleepingQueue;
    
    volatile TVMThreadID currentID;
    volatile int VMTickNum;
    volatile int VMTickMillisecond;
    
    //************************* Mutex Control Block **********************************
    typedef struct MCB
    {
        TVMMutexID mutexID;
        TVMThreadID ownerID;
        bool unlocked;
        vector<TCB*> mhighPriority;
        vector<TCB*> mlowPriority;
        vector<TCB*> mnormalPriority;
    }MCB;
    
    
    vector<MCB*> MutexsVector;    //global mutexs vector
    
    //************************* Memory Pool **********************************
    typedef struct MemoryPool
    {
        TVMMemoryPoolID poolID;
        TVMMemorySize poolSize;
        TVMMemoryPoolIDRef poolIDref;
        TVMMemorySizeRef poolSizeRef;
        uint8_t* blocks;
        void *baseMem;
        TVMMemorySize freeLocation;
    }MemoryPool;
    
    vector<MemoryPool*> MemoryPoolStack;
    const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = 0;
    
    
    //************************* BPB ******************************

        char BS_jmpBoot[3];
        uint8_t BS_OEMName[8];
        uint16_t BPB_BytsPerSec;
        uint16_t BPB_SecPerClus;
        uint16_t BPB_RsvdSecCnt;
        uint16_t BPB_NumFATs;
        uint16_t BPB_RootEntCnt;
        uint16_t BPB_TotSec16;
        uint16_t BPB_Media;
        uint16_t BPB_FATSz16;
        uint16_t BPB_SecPerTrk;
        uint16_t BPB_NumHeads;
        uint16_t BPB_HiddSec;
        uint16_t BPB_TotSec32;
        uint16_t BS_DrvNum;
        uint16_t BS_Reserved1;
        uint16_t BS_BootSig;
        uint32_t BS_VolID;
        uint8_t BS_VolLab[11];
        uint8_t BS_FilSysType[8];
        uint16_t FirstRootSector;
        uint16_t RootDirectorySectors;
        uint16_t FirstDataSector;
        uint16_t ClusterCount;

    
    //************************* Directory ******************************
    typedef struct
    {
        SVMDirectoryEntry entry;
        uint8_t DIR_Attr;
        uint8_t DIR_NTRes;
        uint8_t DIR_CrtTimeTenth;
        uint16_t DIR_CrtTime;
        uint16_t DIR_CrtDate;
        uint16_t DIR_LstAccDate;
        uint16_t DIR_FstClusHI;
        uint16_t DIR_WrtTime;
        uint16_t DIR_WrtDate;
        uint16_t DIR_FstClusLO;
    }Directory;
    
    vector<Directory> globalDirectory;

    //--------------------------------
    typedef struct
    {
        int dirID;
        SVMDirectoryEntry directoryEntry;
        uint16_t firstCluster;
        uint16_t nextCluster;
    }openDir;
    
    vector<openDir> openDirVector;

    //--------------------------------
    int mountfat;


    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Scheduler @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    void Scheduler()
    {
        //cout << "Start Schedule" <<endl;
        TCB* newTCB = new TCB;
        
        if(readyQueue.size() > 0)
        {
            //cout << "if ready Queue >0" <<endl;
            
            for (unsigned int i = 0; i < readyQueue.size(); i++)
            {
                if (readyQueue[i]->tPriority == VM_THREAD_PRIORITY_HIGH)
                {
                    highPriority.push_back(readyQueue[i]);
                    readyQueue.erase(readyQueue.begin() + i);
                }
                
                else if (readyQueue[i]->tPriority == VM_THREAD_PRIORITY_NORMAL)
                {
                    normalPriority.push_back(readyQueue[i]);
                    readyQueue.erase(readyQueue.begin() + i);
                }
                
                else if (readyQueue[i]->tPriority == VM_THREAD_PRIORITY_LOW)
                {
                    lowPriority.push_back(readyQueue[i]);
                    readyQueue.erase(readyQueue.begin() + i);
                }
            }
            
        }
        
        if (highPriority.size() > 0)
        {
            //cout << "if high Queue >0" <<endl;
            
            if ( currentThread->SleepTick !=0 &&currentThread->ThreadState == VM_THREAD_STATE_WAITING )
            {
                sleepingQueue.push_back(currentThread);
            }
            
            else if (currentThread->ThreadState == VM_THREAD_STATE_RUNNING && currentThread != idleThread)
            {
                currentThread->ThreadState = VM_THREAD_STATE_READY;
                
                if (currentThread->tPriority == VM_THREAD_PRIORITY_HIGH)
                    highPriority.push_back(currentThread);
                
                else if (currentThread->tPriority == VM_THREAD_PRIORITY_NORMAL)
                    normalPriority.push_back(currentThread);
                
                else if (currentThread->tPriority == VM_THREAD_PRIORITY_LOW)
                    lowPriority.push_back(currentThread);
                
            }
            
            newTCB = highPriority.front();
            highPriority.erase(highPriority.begin());
            
            TCB* oldTCB = NULL;
            oldTCB= currentThread;
            currentThread = newTCB;
            
            currentThread->ThreadState = VM_THREAD_STATE_RUNNING;
            
            MachineContextSwitch(&(oldTCB->context), &(currentThread->context));
        }
        
        
        else if (normalPriority.size() > 0)
        {
            //cout << "if normal Queue >0" <<endl;
            
            if (currentThread->SleepTick !=0 &&currentThread->ThreadState == VM_THREAD_STATE_WAITING )
            {
                sleepingQueue.push_back(currentThread);
            }
            
            else if (currentThread->ThreadState == VM_THREAD_STATE_RUNNING && currentThread != idleThread)
            {
                currentThread->ThreadState = VM_THREAD_STATE_READY;
                
                if (currentThread->tPriority == VM_THREAD_PRIORITY_HIGH)
                    highPriority.push_back(currentThread);
                
                else if (currentThread->tPriority == VM_THREAD_PRIORITY_NORMAL)
                    normalPriority.push_back(currentThread);
                
                else if (currentThread->tPriority == VM_THREAD_PRIORITY_LOW)
                    lowPriority.push_back(currentThread);
            }
            
            newTCB = normalPriority.front();
            normalPriority.erase(normalPriority.begin());
            
            TCB* oldTCB = NULL;
            oldTCB= currentThread;
            currentThread = newTCB;
            
            currentThread->ThreadState = VM_THREAD_STATE_RUNNING;
            
            MachineContextSwitch(&(oldTCB->context), &(currentThread->context));
            
            
        }
        
        else if (lowPriority.size() > 0)
        {
            //cout << "if low Queue >0" <<endl;
            
            if (currentThread->SleepTick !=0 && currentThread->ThreadState == VM_THREAD_STATE_WAITING )
            {
                //cout << "low thread is sleep" <<endl;
                
                sleepingQueue.push_back(currentThread);
            }
            
            
            else if (currentThread->ThreadState == VM_THREAD_STATE_RUNNING && currentThread != idleThread)
            {
                //cout << "low thread is running" <<endl;
                
                currentThread->ThreadState = VM_THREAD_STATE_READY;
                
                if (currentThread->tPriority == VM_THREAD_PRIORITY_HIGH)
                    highPriority.push_back(currentThread);
                
                else if (currentThread->tPriority == VM_THREAD_PRIORITY_NORMAL)
                    normalPriority.push_back(currentThread);
                
                else if (currentThread->tPriority == VM_THREAD_PRIORITY_LOW)
                    lowPriority.push_back(currentThread);
            }
            
            newTCB = lowPriority.front();
            lowPriority.erase(lowPriority.begin());
            
            TCB* oldTCB = NULL;
            oldTCB= currentThread;
            currentThread = newTCB;
            
            currentThread->ThreadState = VM_THREAD_STATE_RUNNING;
            
            MachineContextSwitch(&(oldTCB->context), &(currentThread->context));
        }
        
        else
        {
            //cout << "else idle thread" <<endl;
            
            newTCB = idleThread;
            
            if (currentThread->SleepTick !=0 && currentThread->ThreadState == VM_THREAD_STATE_WAITING )
            {
                sleepingQueue.push_back(currentThread);
            }
            
            if (currentThread->ThreadState == VM_THREAD_STATE_READY)
            {
                if (currentThread->tPriority == VM_THREAD_PRIORITY_HIGH)
                    highPriority.push_back(currentThread);
                
                else if (currentThread->tPriority == VM_THREAD_PRIORITY_NORMAL)
                    normalPriority.push_back(currentThread);
                
                else if (currentThread->tPriority == VM_THREAD_PRIORITY_LOW)
                    lowPriority.push_back(currentThread);
            }
            
            
            TCB* oldTCB = NULL;
            oldTCB= currentThread;
            currentThread = newTCB;
            
            currentThread->ThreadState = VM_THREAD_STATE_RUNNING;
            MachineContextSwitch(&(oldTCB->context), &(currentThread->context));
            //cout << "End Schedule" <<endl;
            
        }
        
    }

    
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Call Back Function @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    void FilesCallBack (void *cb, int result) //call back to signal when machine is done
    {
        //cout << "Start FileCallBack " <<endl;
        VMTickNum++;

        ((TCB*)cb)->ThreadState = VM_THREAD_STATE_READY;
        ((TCB*)cb)->returnFile = result;
        readyQueue.push_back((TCB*)cb);
        //cout << "   before schedule in FileCallBack " <<endl ;
        
        Scheduler();
        //cout << "END fileCallBack " <<endl <<endl;
        
    }
    
    
    /*  It will be called every tick. This is where you will see if threads should wake up,
     or if another thread should be switched in.
     For all sleep threads in sleepQueue, count down their ticks, once a thread's ticks
     reaches 0, change to ready state and schedule it.
     */
    void AlarmCallback(void *calldata)
    {
        //cout <<" start alarm call back " <<endl;
        VMTickNum++;
        
        if(sleepingQueue.size() >0)
        {
            for(unsigned int pos =0; pos < sleepingQueue.size() ; pos++ )
            {
                if(sleepingQueue[pos]->SleepTick == 0)
                {
                    sleepingQueue[pos]->ThreadState = VM_THREAD_STATE_READY;
                    readyQueue.push_back(sleepingQueue[pos]);
                    sleepingQueue.erase(pos + sleepingQueue.begin());
                    
                    Scheduler();
                }
                
                else if(sleepingQueue[pos]->SleepTick > 0)
                {
                    sleepingQueue[pos]->SleepTick --;
                }
            }
        }
        //cout <<" end alarm call back " <<endl;
    }
    
    // call the entry function from within your skeleton function.
    //The skeleton function is a wrapper for the entry call it is so that you can terminate the thread gracefully in the case where the entry returns.
    //************************************** Skeleton *************************************
    void Skeleton(void *parameter)
    {
        TCB* ske = (TCB*) parameter;
        MachineEnableSignals();
        //cout << "Start skeleton" << endl;
        
        //call entry of thread
        ske-> ThreadEntry(ske->entry);
        VMThreadTerminate(ske->ThreadID);
        //cout << "End skeleton" << endl <<endl;
        
    }
    
    //************************************** Skeleton *************************************
    void IdleFunction(void *idle)
    {
        //cout << "Start and end idlefunction" << endl <<endl;
        MachineEnableSignals();
        while(true);
    }
    
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Project 4 @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    
    
    void sector(int index, uint8_t** data)
    {
        MachineFileSeek(mountfat, index, 0, FilesCallBack, currentThread);
        currentThread->ThreadState = VM_THREAD_STATE_WAITING;
        Scheduler();
        
        MachineFileRead(mountfat, (void*) *data, 512, FilesCallBack, currentThread);
        currentThread->ThreadState = VM_THREAD_STATE_WAITING;
        Scheduler();
    }
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ VM START @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    TVMStatus VMStart(int tickms, TVMMemorySize heapsize, TVMMemorySize sharedsize, const char *mount, int argc, char *argv[])
    {
        //cout << "Start VMStart" << endl;
        
        VMTickMillisecond = tickms; //store tickms to use for VMTickms
        TVMMainEntry VMMain = VMLoadModule(argv[0]);
        MachineEnableSignals();
        
        uint8_t* base = (uint8_t*)MachineInitialize((size_t)sharedsize);
        
        //request machine alarm( it requests that the callback is called at the period specified by the time)
        MachineRequestAlarm(tickms * 1000, AlarmCallback, NULL);
        
        if (VMMain != NULL)
        {
            //cout << "   if VNNain is not NULL!" << endl;
            
            /*******set up the main thread and push it into the thread vector*******/
            TCB* newThread = new TCB();
            newThread->ThreadState = VM_THREAD_STATE_RUNNING;
            newThread->tPriority = VM_THREAD_PRIORITY_NORMAL;
            newThread->SleepTick = 0;
            newThread->ThreadID = 0;
            newThread->ThreadEntry = NULL;
            newThread->memorySize = 0;
            
            ThreadsVector.push_back(newThread);
            currentThread = newThread;
            idleThread->base = NULL;
            
            currentThread->ThreadID = 0;
            
            /*************************set up the idle thread*************************/
            idleThread->ThreadState = VM_THREAD_STATE_READY;
            idleThread->tPriority = 0;
            idleThread->SleepTick = 0;
            idleThread->ThreadID = 1;
            uint8_t *stacker = new uint8_t[0x100000];
            idleThread->base = stacker;
            idleThread->ThreadEntry = IdleFunction;
            idleThread->memorySize = 0x100000;
            
            //creates a context that will enter in the function specified by entry and passing it the parameter param
            MachineContextCreate(&(idleThread->context), IdleFunction, NULL, idleThread->base, idleThread->memorySize);
            
            ThreadsVector.push_back(idleThread);
            
            /********************* set up system memory pool***********************/
            //heapsize is the size of system memory pool
            unsigned int numBlocks = heapsize/64 + (heapsize % 64);
            
            MemoryPool *SystemPool = new MemoryPool();
            SystemPool-> poolID = VM_MEMORY_POOL_ID_SYSTEM;
            SystemPool->poolSize = heapsize;
            SystemPool->baseMem = new uint8_t[heapsize];
            SystemPool->blocks = new uint8_t[numBlocks];
            
            MemoryPoolStack.push_back(SystemPool);
            
            /********************* set up share memory pool***********************/
            unsigned int numBlock = sharedsize/64;
            
            MemoryPool *shareMemPool = new MemoryPool();
            shareMemPool-> poolID = 1;
            shareMemPool->poolSize = sharedsize;
            shareMemPool->baseMem = base;
            shareMemPool->blocks = new uint8_t[numBlock];
            
            MemoryPoolStack.push_back(shareMemPool);
            
            /********************* filesystem ***********************/

            MachineFileOpen(mount, O_RDWR, 0, FilesCallBack, currentThread);
            currentThread->ThreadState = VM_THREAD_STATE_WAITING;
            Scheduler();
            
            mountfat = currentThread->returnFile;
            uint8_t* BPB_Pointer;
            VMMemoryPoolAllocate(1, 512, (void**) &BPB_Pointer);
            
            MachineFileRead(mountfat, (void*)BPB_Pointer, 512, FilesCallBack, currentThread);
            currentThread->ThreadState = VM_THREAD_STATE_WAITING;
            Scheduler();
            
            //-------------------------BPB-------------------------------
            for(int i = 0; i < 3; i++) {BS_jmpBoot[i] = BPB_Pointer[i];}
            for (int i = 0; i < 8; i++) { BS_OEMName[i] = BPB_Pointer[i+3];}
            BPB_BytsPerSec = BPB_Pointer[11] + (((uint16_t)BPB_Pointer[12]) << 8);
            BPB_SecPerClus = BPB_Pointer[13];
            BPB_RsvdSecCnt = BPB_Pointer[14] + (((uint16_t)BPB_Pointer[15]) << 8);
            BPB_NumFATs = BPB_Pointer[16];
            BPB_RootEntCnt = BPB_Pointer[17] + (((uint16_t)BPB_Pointer[18]) << 8);
            BPB_TotSec16 = BPB_Pointer[19] + (((uint16_t)BPB_Pointer[20]) << 8);
            BPB_Media = BPB_Pointer[21];
            BPB_FATSz16 = BPB_Pointer[22] + (((uint16_t)BPB_Pointer[23]) << 8);
            BPB_SecPerTrk = BPB_Pointer[24] + (((uint16_t)BPB_Pointer[25]) << 8);
            BPB_NumHeads = BPB_Pointer[26] + (((uint16_t)BPB_Pointer[27]) << 8);
            BPB_HiddSec = BPB_Pointer[28] + (((uint16_t)BPB_Pointer[29]) << 8) + (((uint32_t)BPB_Pointer[30]) << 16) + (((uint32_t)BPB_Pointer[31]) << 24);
            BPB_TotSec32 = BPB_Pointer[32] + (((uint16_t)BPB_Pointer[33]) << 8) + (((uint32_t)BPB_Pointer[34]) << 16) + (((uint32_t)BPB_Pointer[35]) << 24);
            BS_DrvNum = BPB_Pointer[36];
            BS_Reserved1 = BPB_Pointer[37];
            BS_BootSig = BPB_Pointer[38];
            BS_VolID = BPB_Pointer[39] + (((uint16_t)BPB_Pointer[40]) << 8) + (((uint16_t)BPB_Pointer[41]) << 8) + (((uint16_t)BPB_Pointer[42]) << 8);
            for(int i = 0; i < 11; i++) {BS_VolLab[i] = BPB_Pointer[43+i];}
            for(int i = 0; i < 8; i++) {BS_FilSysType[i] = BPB_Pointer[54 + i];}
            
            FirstRootSector = BPB_RsvdSecCnt + BPB_NumFATs*BPB_FATSz16;
            RootDirectorySectors = (BPB_RootEntCnt*32)/512;
            FirstDataSector = FirstRootSector + RootDirectorySectors;
            ClusterCount = (BPB_TotSec32 - FirstDataSector) / BPB_SecPerClus;
            
            VMMemoryPoolDeallocate(1, BPB_Pointer);
            
            //-------------------------ROOT-------------------------------
            
            uint8_t* block;
            SVMDirectoryEntry dir;
            Directory newDir;
            int rootSector = FirstRootSector*512;

            for(int numCluster = 0; numCluster < RootDirectorySectors; numCluster++)
            {
                VMMemoryPoolAllocate(1, 512, (void**) &block);
                sector(rootSector, &block);

                for(int offset = 0; offset < 512; offset+=32)
                {
                    
                   if(block[offset + 11] == 0x0f)  //skip long name
                       continue;
                    
                    if (block[offset] == 0x00)
                        continue;
                    
                    //----------------Display Short File Name-------------------
                    for(int i = 0; i < 11; i++)
                    {
                        if(i < 8)
                            dir.DShortFileName[i] = block[i+offset];
                        
                        else  if(i >= 8 )
                            dir.DShortFileName[i+1] = block[i+offset];
                    }
                    
                    
                    dir.DShortFileName[8] = '.';
                    
                    
                    for(int i = 0; i < string((const char*)dir.DShortFileName).size(); i++)
                    {
                        if(dir.DShortFileName[i] == '.' && dir.DShortFileName[i + 1] == ' ')
                        {
                            dir.DShortFileName[i] = ' ';
                        }
                        else if(dir.DShortFileName[i] == ' ')
                        {
                            dir.DShortFileName[i] = 127;
                            //dir.DShortFileName[12] = 0x00;
                        }
                    }
                    
                    cout << "|" << dir.DShortFileName << "|" <<endl;

                    for(int i = 0; i < 11; i++)
                    {
                        if(dir.DShortFileName[i] != ' ' && dir.DShortFileName[i + 1] == ' ')
                        dir.DShortFileName[i+1] = 0x00;
                    }

                    //----------------Display Day and Time-------------------

                    dir.DAttributes = block[11 + offset];
                    dir.DCreate.DHundredth = block[13 + offset];
                    dir.DSize = block[28 + offset] + (((uint16_t)block[29 + offset]) << 8) + (((uint32_t)block[30 + offset]) << 16) + (((uint32_t)block[31 + offset]) << 24);
                    
                    //CREAT DATE
                    dir.DCreate.DYear = (block[16 + offset] >> 9) + ((block[17 + offset]) >> 1) + 1980;
                    dir.DCreate.DMonth = ((block[16 + offset] >> 5)+ ((block[17 + offset]) << 3)) & 0xF;
                    dir.DCreate.DDay = (block[16 + offset] + ((block[17 + offset]) << 8)) & 0x1F;
                    
                    //ACCESS DATE
                    dir.DAccess.DYear = (block[18 + offset] >> 9) + ((block[19 + offset]) >> 1) + 1980;
                    dir.DAccess.DMonth = ((block[18 + offset] >> 5)+ ((block[19 + offset]) << 3)) & 0xF;
                    dir.DAccess.DDay = (block[18 + offset] + ((block[19 + offset]) << 8)) & 0x1F;
                    
                    
                    //MODIFY DATE
                    dir.DModify.DYear =  (block[24 + offset] >> 9) + ((block[25 + offset]) >> 1) + 1980;
                    dir.DModify.DMonth = ((block[24 + offset] >> 5) + ((block[25 + offset]) << 3)) & 0xF;
                    dir.DModify.DDay = ((block[24 + offset] + ((block[25 + offset]) << 8)) & 0x1F);
                    
                    
                    //CREAT TIME
                    dir.DCreate.DHour = (block[14 + offset] >> 11) + ((block[15 + offset]) >>3);
                    dir.DCreate.DMinute = ((block[14 + offset] >> 5) + ((block[15 + offset]) << 3)) & 0x2F;
                    dir.DCreate.DSecond = ((block[14 + offset] + ((block[15 + offset]) << 8)) & 0x1F) << 1;

                    
                    //MODIFY TIME
                    dir.DModify.DHour = (block[22 + offset] >> 11) + ((block[23 + offset]) >>3);
                    dir.DModify.DMinute = ((block[22 + offset] >> 5) + ((block[23 + offset]) << 3)) & 0x2F;
                    dir.DCreate.DSecond = ((block[22 + offset] + ((block[23 + offset]) << 8)) & 0x1F) << 1;

                    newDir.entry = dir;
                    if (dir.DAttributes != 0x0f)
                        globalDirectory.push_back(newDir);
                    
                }
                VMMemoryPoolDeallocate(1, block);
                rootSector += 512;
            }
            

            /**********************************************************************/
            VMMain(argc, argv);       //run the module that we just loaded
            
            MachineTerminate();
            VMUnloadModule();
            
            return VM_STATUS_SUCCESS;
        }
        
        else
            return VM_STATUS_FAILURE;
        
    }
    
    
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ VM Directory @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    
    //*************************************Open Directory****************************************
    TVMStatus VMDirectoryOpen(const char *dirname, int *dirdescriptor)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if(dirname == NULL || dirdescriptor == NULL)
        {
            //MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        if(strcmp(dirname, "/") == 0){
            
            openDir temp;
            temp.firstCluster = 0;
            //storing the first directory entry in
            temp.directoryEntry = globalDirectory[0].entry;
            
            //cerr << globalDirectory[0].entry.DShortFileName<< endl;
            temp.dirID = openDirVector.size() + 3;
            temp.nextCluster = 0;
            *dirdescriptor = openDirVector.size() + 3;
            openDirVector.push_back(temp);
            
            
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
            
            // return success;
        }
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
        
    }
    
    
    //*************************************Close Directory****************************************
    TVMStatus VMDirectoryClose(int dirdescriptor)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        //When a thread calls VMDirectoryClose() it blocks in the wait state VM_THREAD_STATE_WAITING
        //if the closing of the directory cannot be completed immediately.
        currentThread->ThreadState = VM_THREAD_STATE_WAITING;
        
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
        
    }
    
    
    //*************************************Read Directory****************************************
    TVMStatus VMDirectoryRead(int dirdescriptor, SVMDirectoryEntryRef dirent)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);

        if(dirent == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        if(dirdescriptor > 0 && dirdescriptor < (int)openDirVector.size())
        {
            return VM_STATUS_FAILURE;
        }
        
        if (openDirVector[dirdescriptor-3].nextCluster >= (int)globalDirectory.size()){
            return VM_STATUS_FAILURE;
        }
        
        *dirent = openDirVector[dirdescriptor-3].directoryEntry;
        
        openDirVector[dirdescriptor-3].nextCluster++;
        openDirVector[dirdescriptor-3].directoryEntry = globalDirectory[openDirVector[dirdescriptor-3].nextCluster].entry;
        

        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
        
    }
    
    //*************************************Current Directory****************************************
    TVMStatus VMDirectoryCurrent(char *abspath)//
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if(abspath == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        //1. VMDirectoryCurrent() attempts to place the absolute path of the current working
        //directory in the location specified by abspath.
        else
            VMStringCopy(abspath, "/");
        
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    
    //*************************************Change Directory****************************************
    TVMStatus VMDirectoryChange(const char *path)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if(path == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        //1. if path specified by path does not exist, VMDirectoryChange() returns VM_STATUS_FAILURE
        
        //2. VMDirectoryChange() attempts to change the current working directory of the mounted
        //FAT file system to the name specified by path.

        if (strcmp(path, "."))
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
        

        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
        
    }
    
    
    //*************************************Create Directory****************************************
    TVMStatus VMDirectoryCreate(const char *dirname)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    
    //*************************************Unlink Directory****************************************
    TVMStatus VMDirectoryUnlink(const char *path)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    
    //*************************************Rewind Directory****************************************
    TVMStatus VMDirectoryRewind(int dirdescriptor)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ VMemory Pool @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //*************************************Pool Create****************************************
    TVMStatus VMMemoryPoolCreate(void *base, TVMMemorySize size, TVMMemoryPoolIDRef memory)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        //cout << "Start pool create" << endl;
        
        if(base == NULL || memory == NULL || size ==0)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        else
        {
            //The memory pool identifier is put into the location specified by the memory parameter.
            *memory = MemoryPoolStack.size();
            
            MemoryPool* systemPool = new MemoryPool();
            systemPool->poolSize = size;
            systemPool->baseMem = (uint8_t*)base;
            systemPool->poolID = MemoryPoolStack.size();
            systemPool->freeLocation = size;
            systemPool->blocks = new uint8_t[size/64];
            
            MemoryPoolStack.push_back(systemPool);
            
            MachineResumeSignals(&sigState);
            //cout << "End pool Create" << endl <<endl;
            return VM_STATUS_SUCCESS;
        }
    }
    
    //**************************************Pool Delete****************************************
    TVMStatus VMMemoryPoolDelete(TVMMemoryPoolID memory)
    {
        //cout << "Start pool Delete" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        //If the memory pool specified by memory is not a valid memory pool, VM_STATUS_ERROR_INVALID_PARAMETER is returned.
        if(MemoryPoolStack[memory] == NULL|| memory < 0)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        else
            MemoryPoolStack.erase(MemoryPoolStack.begin() + memory); //erase from the memory pool
        
        MachineResumeSignals(&sigState);
        //cout << "End pool Delete" << endl <<endl;
        return VM_STATUS_SUCCESS;
    }
    
    
    //*************************************Pool Query****************************************
    TVMStatus VMMemoryPoolQuery(TVMMemoryPoolID memory, TVMMemorySizeRef bytesleft)
    {
        //cout << "Start pool Query" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if(MemoryPoolStack[memory] == NULL|| memory < 0 || bytesleft == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        else
        {
            *bytesleft = MemoryPoolStack[memory]->freeLocation;
            
            MachineResumeSignals(&sigState);
            //cout << "End pool Query" << endl <<endl;
            return VM_STATUS_SUCCESS;
        }
    }
    
    
    //************************************Pool Allocate****************************************
    TVMStatus VMMemoryPoolAllocate(TVMMemoryPoolID memory, TVMMemorySize size, void **pointer)
    {
        //cout << "Start pool Allocate" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if(size == 0 || MemoryPoolStack[memory] == NULL || pointer == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        else
        {
            //cout << "   Pool Allocate valid!" << endl;
            
            //size here is the size of allocate data need
            //allocateDataChunk is the number of chunks of allocated data (each chunk is 64 bytes)
            unsigned int allocatedDataChunk = size/64  + (size % 64);
            unsigned int systemPoolChunk = MemoryPoolStack[memory]->poolSize/64;
            unsigned char *base = (uint8_t*)MemoryPoolStack[memory]->baseMem;
            unsigned int numFreeChunks = 0;
            
            if( systemPoolChunk < allocatedDataChunk)
            {
                //cout << "   Error resources!" << endl;
                MachineResumeSignals(&sigState); //resume signals
                return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
            }
            
            else if( systemPoolChunk >= allocatedDataChunk)
            {
                //cout << "   systemPoolChunk >= allocatedDataChunk!" << endl;
                unsigned int numFreeChunk = 0;
                for(unsigned int i = 0; i < systemPoolChunk; i++)
                {
                    if(MemoryPoolStack[memory]->blocks[i] == 0)
                        numFreeChunk++;
                }
                
                if(numFreeChunk < allocatedDataChunk)
                {
                    MachineResumeSignals(&sigState);
                    //cout << "End pool Allocate 1" << endl <<endl;
                    return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
                }
            }
            
            for(unsigned int i = 0; i < systemPoolChunk; i++)
            {
                if(MemoryPoolStack[memory]->blocks[i] == 0)
                {
                    numFreeChunks++;
                }
                
                if(numFreeChunks == allocatedDataChunk)
                {
                    //cout << "   numFreeChunks == allocatedDataChunk!" << endl;
                    for(unsigned int j = 0 ; j < allocatedDataChunk; j++)
                    {
                        MemoryPoolStack[memory]->freeLocation -= 64;
                    }
                    
                    *pointer = base + 64*(i - allocatedDataChunk + 1);
                    
                    MemoryPoolStack[memory]->blocks[i] = allocatedDataChunk; //fill free chunks with those allocated datas
                    
                    MachineResumeSignals(&sigState);
                    return VM_STATUS_SUCCESS;
                }
            }
            
            //cout << "End pool Allocate 2" << endl <<endl;
            MachineResumeSignals(&sigState);
            return VM_STATUS_FAILURE;
        }
    }
    
    
    //**********************************Pool Deallocate ****************************************
    TVMStatus VMMemoryPoolDeallocate(TVMMemoryPoolID memory, void *pointer)
    {
        //cout << "Start pool deallocate" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        unsigned int base = *(uint8_t*)&MemoryPoolStack[memory]->baseMem;
        
        if(MemoryPoolStack[memory] == NULL || pointer == NULL)
        {
            MachineResumeSignals(&sigState);
            //cout << "End pool deallocate 1" << endl <<endl;
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        else
        {
            //cout << "   pool deallocate valid!" << endl;
            for( unsigned int i=0; i < (*(uint8_t*)&pointer - base)/64 + 1; i++)
            {
                MemoryPoolStack[memory]->blocks[i] = 0;
                MemoryPoolStack[memory]->freeLocation += 64;
            }
            
            //cout << "End pool deallocate 2" << endl <<endl;
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
    }
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ VM FILES @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //****************************** VMFileRead OK *************************************
    TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
    {
        //cout << "Start fileRead" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if(data == NULL || length == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        else
        {
            //cout << "   fileRead valid!" << endl;
            
            ///thread blocks in a wait state until either suc or unsucessful openng of the file is completed
            currentThread->ThreadState = VM_THREAD_STATE_WAITING;
            
            void *shareMemory = NULL;
            
            if(*length <= 512)
            {
                //cout << "   if length <= 512!" << endl;
                VMMemoryPoolAllocate(1, *length, &shareMemory);
                //cout << "  Start MachineFileRead 1!" << endl;
                
                MachineFileRead(filedescriptor, shareMemory, *length, FilesCallBack, currentThread);
                
                //cout << "  End MachineFileRead and start Schedule 1!" << endl;
                
                Scheduler();
                
                memcpy((char*)data, shareMemory, (size_t)*length);
                
                *length = currentThread->returnFile;
            }
            
            else if(*length >512)
            {
                //cout << "   if length > 512!" << endl;
                int wholeLen = *length;
                *length = 0;
                
                while(wholeLen > 0)
                {
                    VMMemoryPoolAllocate(1, 512, &shareMemory);
                    //cout << "  Start MachineFileRead 2!" << endl;
                    
                    MachineFileRead(filedescriptor, shareMemory, 512, FilesCallBack, currentThread);
                    
                    //cout << "  End MachineFileRead and start Schedule 2!" << endl;
                    
                    Scheduler();
                    
                    memcpy((char*)data, shareMemory, 512);
                    
                    *length += 512;
                    
                    wholeLen -= 512;
                    
                }
            }
            
            VMMemoryPoolDeallocate(1, shareMemory);
            
            //cout << "END file REad " <<endl <<endl;
            MachineResumeSignals(&sigState);
            if((currentThread->returnFile) > 0)
                return VM_STATUS_SUCCESS;
            else
                return VM_STATUS_FAILURE;
            
        }
    }
    
    
    
    //****************************** VMFileWrite OK *************************************
    TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
    {
        //cout << "Start fileWrite" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        //If data or length parameters are NULL, VMFileWrite() returns VM_STATUS_ERROR_INVALID_PARAMETER.
        if(data == NULL || length == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        else
            
        {
            ///thread blocks in a wait state until either suc or unsucessful openng of the file is completed
            currentThread->ThreadState = VM_THREAD_STATE_WAITING;
            
            void *shareMemory = NULL;
            int pointer = 0;
            
            if(*length <= 512)
            {
                //cout << "   if length <= 512!" << endl;
                VMMemoryPoolAllocate(1, *length, &shareMemory);
                
                memcpy(shareMemory, (char*)data, (size_t)*length);
                
                //cout << "  Start MachineFileWrite 1!" << endl;
                MachineFileWrite(filedescriptor, shareMemory, *length, FilesCallBack, currentThread);
                
                //cout << "  End MachineFileWrite and start Schedule 1!" << endl;
                
                Scheduler();
                
                *length = currentThread->returnFile;
            }
            
            else if(*length >512)
            {
                //cout << "   if length > 512!" << endl;
                int wholeLen = *length;
                *length = 0;
                
                while(wholeLen > 0)
                {
                    VMMemoryPoolAllocate(1, 512, &shareMemory);
                    
                    memcpy(shareMemory, (char*)data + pointer, 512);
                    
                    //cout << "  Start MachineFileWrite 2!" << endl;
                    MachineFileWrite(filedescriptor, shareMemory, 512, FilesCallBack, currentThread);
                    
                    //cout << "  End MachineFileWrite and start Schedule 2!" << endl;
                    
                    Scheduler();
                    
                    *length += 512;
                    
                    wholeLen -= 512;
                    
                    pointer += 512;
                }
            }
            
            VMMemoryPoolDeallocate(1, shareMemory);
            
            //cout << "END file Write " <<endl <<endl;
            MachineResumeSignals(&sigState);
            if((currentThread->returnFile) > 0)
                return VM_STATUS_SUCCESS;
            else
                return VM_STATUS_FAILURE;
        }
    }
    
    
    //****************************** VMFileOpen OK *************************************
    TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        
        if (filename == NULL || filedescriptor == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        else
        {
            ///thread blocks in a wait state until either suc or unsucessful openng of the file is completed
            currentThread->ThreadState = VM_THREAD_STATE_WAITING;
            MachineFileOpen(filename, flags, mode, FilesCallBack, currentThread);
            
            Scheduler();
            
            //The file descriptor of newly opened file will be passed in to callback function as the result.
            *filedescriptor = currentThread->returnFile;
            
            MachineResumeSignals(&sigState);
            if (*filedescriptor > 0)
                return VM_STATUS_SUCCESS;
            else
                return VM_STATUS_FAILURE;
        }
    }
    
    
    //****************************** VMFileClose OK*************************************
    TVMStatus VMFileClose(int filedescriptor)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        ///thread blocks in a wait state until either suc or unsucessful openng of the file is completed
        currentThread->ThreadState = VM_THREAD_STATE_WAITING;
        
        MachineFileClose(filedescriptor, FilesCallBack, currentThread);
        Scheduler();
        
        MachineResumeSignals(&sigState);
        if(currentThread -> returnFile > 0)
            return VM_STATUS_SUCCESS;
        
        else
            return VM_STATUS_FAILURE;
        
    }
    
    
    //****************************** VMFileSeek OK *************************************
    TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)  //whence: location
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        ///thread blocks in a wait state until either suc or unsucessful openng of the file is completed
        currentThread->ThreadState = VM_THREAD_STATE_WAITING;
        MachineFileSeek(filedescriptor, offset, whence, FilesCallBack, currentThread);
        Scheduler();
        
        *newoffset = currentThread->returnFile;
        
        MachineResumeSignals(&sigState);
        if ((currentThread->returnFile) > 0)
            return VM_STATUS_SUCCESS;
        else
            return VM_STATUS_FAILURE;
    }
    
    
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ VM TICK @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //********* ok *****************************VMTickMS****************************************
    TVMStatus VMTickMS(int *tickmsref)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if (tickmsref != NULL)
        {
            *tickmsref = VMTickMillisecond;
        }
        
        else
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    
    //********** ok *************************VMTickCount***************************************
    TVMStatus VMTickCount(TVMTickRef tickref)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if (tickref != NULL)
        {
            *tickref = VMTickNum;
        }
        
        else
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    
    
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ VM THREAD @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2@@
    //****************************** VMThreadSleep OK *************************************
    TVMStatus VMThreadSleep(TVMTick tick)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        //cout << "Start VM Thread Sleep " << endl;
        
        if ( tick == VM_TIMEOUT_INFINITE )
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        else
        {
            /* if( tick == VM_TIMEOUT_IMMEDIATE)
             {
             currentThread->ThreadState = VM_THREAD_STATE_READY;
             readyQueue.push_back(currentThread);
             Scheduler();
             }*/
            
            if( tick != VM_TIMEOUT_IMMEDIATE)
            {
                currentThread->SleepTick = tick;
                if(tick > 0)
                {
                    //cout << "If tick > 0 " <<endl;
                    currentThread->ThreadState = VM_THREAD_STATE_WAITING;
                    Scheduler();
                }
            }
            
            MachineResumeSignals(&sigState);
            //cout << "END VM Thread Sleep " << endl <<endl;
            return VM_STATUS_SUCCESS;
        }
        
    }
    
    
    //**************************************VMThreadCreate*************************************
    TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if (entry == NULL || tid == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        else
        {
            TCB *newTCB = new TCB();
            
            newTCB->entry = param;
            newTCB->ThreadEntry = entry;
            //memsize in VMThreadCreate same as stacksize in MachineContextCreate
            newTCB->base = new uint8_t[memsize]; // stacksize use uint8_t.
            newTCB->memorySize = memsize;
            newTCB->ThreadState = VM_THREAD_STATE_DEAD;
            newTCB->tPriority = prio;
            *tid = ThreadsVector.size();
            newTCB->ThreadID = *tid;
            
            ThreadsVector.push_back(newTCB);
            
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
    }
    
    
    //*************************************VMThreadState OK************************************
    TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        if(stateref == NULL)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        else
        {
            if(ThreadsVector[thread] == NULL || thread < 0)
            {
                MachineResumeSignals(&sigState);
                return VM_STATUS_ERROR_INVALID_ID;
            }
            else
                *stateref = ThreadsVector[thread]->ThreadState;
            
            
        }
        
        MachineResumeSignals(&sigState);
        
        return VM_STATUS_SUCCESS;
    }
    
    
    //************************************** VMThreadActivate *************************************
    TVMStatus VMThreadActivate(TVMThreadID thread)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        //cout << "           ENTER ThreadActivate " <<endl;
        
        if(thread< 0)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
        
        else
        {
            ///cout << "Thread is VALID " <<endl;
            
            for(unsigned int i =0; i < ThreadsVector.size(); i++)
            {
                //cout << "The Thread at position i is DEAD Thread" <<endl;
                if(ThreadsVector[i]->ThreadID == thread)
                {
                    //cout << "The Thread at position i is the mission dead thread" <<endl;
                    
                    if (ThreadsVector[i]->ThreadState != VM_THREAD_STATE_DEAD && currentThread-> ThreadState == VM_THREAD_STATE_RUNNING)
                    {
                        if(currentThread->tPriority < ThreadsVector[i]->tPriority)
                            Scheduler();
                        
                        else
                        {
                            MachineResumeSignals(&sigState);
                            return VM_STATUS_ERROR_INVALID_STATE;
                        }
                    }
                    
                    else //if thread is running then put it to schedule
                    {
                        //make new context for activated Thread, will enter in the function specified by entry and passing it the parameter param
                        MachineContextCreate(&ThreadsVector[i]->context, Skeleton, ThreadsVector[i], ThreadsVector[i]->base,ThreadsVector[i]->memorySize);
                        
                        ThreadsVector[i]->ThreadState = VM_THREAD_STATE_READY;
                        readyQueue.push_back(ThreadsVector[i]);
                    }
                }
                
            }
        }
        
        MachineResumeSignals(&sigState);
        //cout << "           END ThreadActivate\n" <<endl;
        return VM_STATUS_SUCCESS;
    }
    
    
    //************************************** VMThreadTernimate*************************************
    TVMStatus VMThreadTerminate(TVMThreadID thread)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if (ThreadsVector.at(thread)->ThreadState == VM_THREAD_STATE_DEAD)
        {
            MachineResumeSignals(&sigState);
            
            return VM_STATUS_ERROR_INVALID_STATE;
        }
        else if(ThreadsVector.size() < thread)
        {
            MachineResumeSignals(&sigState);
            
            return VM_STATUS_ERROR_INVALID_ID;
        }
        ThreadsVector.at(thread)->ThreadState = VM_THREAD_STATE_DEAD;
        
        Scheduler();
        MachineResumeSignals(&sigState);
        
        //cout << "End THREADterminate\n" <<endl;
        return VM_STATUS_SUCCESS;
    }
    
    
    //************************************** VMThreadDelete *************************************
    //check
    TVMStatus VMThreadDelete(TVMThreadID thread)
    {
        //cout <<"   START VM Thread Deletion" <<endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        
        if(ThreadsVector[thread]->ThreadState == VM_THREAD_STATE_DEAD)
        {
            ThreadsVector.erase(ThreadsVector.begin() + thread);
            //Replace the thread's location with NULL to avoid changing the index of each element.
            ThreadsVector[thread] = NULL;
        }
        
        else if (ThreadsVector[thread]->ThreadState != VM_THREAD_STATE_DEAD)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_STATE;
        }
        
        else if (ThreadsVector.size() < thread)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
        
        MachineResumeSignals(&sigState);
        //cout <<"      END VM Thread Deletion" <<endl;
        
        return VM_STATUS_SUCCESS;
    }
    
    
    
    //****** ok ******************************** VMThreadID *************************************
    TVMStatus VMThreadID(TVMThreadIDRef threadref)
    {
        //cout << "start Thread ID" <<endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if(threadref != NULL)
        {
            *threadref = currentThread->ThreadID;
        }
        else
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        MachineResumeSignals(&sigState);
        //cout << "end Thread ID function" <<endl;
        return VM_STATUS_SUCCESS;
    }
    
    
    
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@MUTEX@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    
    //***********************************VMMutexCreate***************************************
    TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
    {
        //cout << "start Mutex Create" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if (mutexref == NULL)
        {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        *mutexref = MutexsVector.size();
        MCB *newMCB = new MCB();
        newMCB->mutexID = *mutexref;
        newMCB->unlocked = true;
        
        MutexsVector.push_back(newMCB);
        MachineResumeSignals(&sigState);
        //cout << "end Mutex Create function" << endl;
        return VM_STATUS_SUCCESS;
    }
    
    //*********************************VMMutexAcquire****************************************
    TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
    {
        //cout << "start VMMutexAcquire" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if (mutex < 0 || mutex > MutexsVector.size())
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
        else if (timeout == VM_TIMEOUT_IMMEDIATE && MutexsVector[mutex]->ownerID >= 0)
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_FAILURE;
        }
        else if (timeout == VM_TIMEOUT_IMMEDIATE && MutexsVector[mutex]->ownerID < 0)
        {
            //cout << "vm_status_success1" << endl;
            MutexsVector[mutex]->ownerID = currentID;
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
        
        else if (MutexsVector[mutex]->unlocked == true)
        {
            ThreadsVector[currentID]->ThreadState = VM_THREAD_STATE_WAITING;
            ThreadsVector[currentID]->SleepTick = timeout;
            sleepingQueue.push_back(ThreadsVector[currentID]);
            
            if (ThreadsVector[currentID]->tPriority == VM_THREAD_PRIORITY_LOW)
            {
                //	cout << "push low to low priority" << endl;
                ThreadsVector[currentID]->returnMutexIndex = MutexsVector[mutex]->mlowPriority.size();
                MutexsVector[mutex]->mlowPriority.push_back(ThreadsVector[currentID]);
            }
            else if (ThreadsVector[currentID]->tPriority == VM_THREAD_PRIORITY_NORMAL)
            {
                //	cout << "push normal to normal priority" << endl;
                ThreadsVector[currentID]->returnMutexIndex = MutexsVector[mutex]->mnormalPriority.size();
                MutexsVector[mutex]->mnormalPriority.push_back(ThreadsVector[currentID]);
            }
            else if (ThreadsVector[currentID]->tPriority == VM_THREAD_PRIORITY_HIGH)
            {
                //	cout << "push high to high priority" << endl;
                ThreadsVector[currentID]->returnMutexIndex = MutexsVector[mutex]->mhighPriority.size();
                MutexsVector[mutex]->mhighPriority.push_back(ThreadsVector[currentID]);
            }
        }
        
        MachineResumeSignals(&sigState);
        //cout << "end Mutex Acquire function" << endl;
        return VM_STATUS_SUCCESS;
    }
    
    //*****************************************VMMutexRelease*******************************
    
    TVMStatus VMMutexRelease(TVMMutexID mutex)
    {
        //cout << "start VMMutexRelease" << endl;
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        
        
        if (mutex < 0 || mutex > MutexsVector.size())
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
        MCB *newMCB = new MCB();
        TCB *newTCB = new TCB();
        
        newMCB->unlocked = true;
        newMCB = MutexsVector[mutex];
        ThreadsVector[newMCB->ownerID]->ThreadState = VM_THREAD_STATE_READY;
        
        if (!MutexsVector[mutex]->mhighPriority.empty())
        {
            //	cout << "Testing High Priority" << endl;
            newTCB = MutexsVector[mutex]->mhighPriority.front();
            MutexsVector[mutex]->mhighPriority.pop_back();
            Scheduler();
        }
        
        else if (!MutexsVector[mutex]->mnormalPriority.empty())
        {
            //	cout << "Testing Medium Priority" << endl;
            newTCB = MutexsVector[mutex]->mnormalPriority.front();
            MutexsVector[mutex]->mnormalPriority.pop_back();
            Scheduler();
        }
        else if (!MutexsVector[mutex]->mlowPriority.empty())
        {
            //	cout << "Testing Low Priority" << endl;
            newTCB = MutexsVector[mutex]->mlowPriority.front();
            MutexsVector[mutex]->mlowPriority.pop_back();
            Scheduler();
        }
        currentID = newTCB->ThreadID;
        
        //MachineContextSwitch() switches context to a previously saved the machine context that is specified by the parameter mcntxnew, and stores the current context in the parameter specified by mctxold.
        MachineContextSwitch(&ThreadsVector[currentID]->context, &newTCB->context);
        
        MachineResumeSignals(&sigState);
        //cout << "end Mutex Release function" << endl;
        return VM_STATUS_SUCCESS;
        
        //cout << "end Mutex Release function" << endl;
    }
    
    //**************************************VMMutexQuery************************************
    TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if (ownerref == NULL)
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        else if (mutex < 0 || mutex > MutexsVector.size())
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
        else
        {
            *ownerref = MutexsVector[mutex]->ownerID;
        }
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    
    //************************************VMMutexDelete**************************************
    TVMStatus VMMutexDelete(TVMMutexID mutex)
    {
        TMachineSignalState sigState;
        MachineSuspendSignals(&sigState);
        
        if (mutex < 0 || mutex > MutexsVector.size())
        {
            MachineResumeSignals(&sigState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
        else if (MutexsVector[mutex]->ownerID != NULL )
        {
            MachineResumeSignals(&sigState);
            
            return VM_STATUS_ERROR_INVALID_STATE;
        }
        else
        {
            delete MutexsVector[mutex];
            MutexsVector[mutex] = NULL;
        }
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}



