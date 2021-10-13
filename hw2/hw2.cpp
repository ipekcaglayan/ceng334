
#include <iostream>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "writeOutput.h"
#include "helper.h"
#include <queue>


using namespace std;

class Sender{
    public:
        int id;
        int timeBtwTwoPackages;
        int totalPackages;
        int assignedHubId;


};
class Hub{
    public:
        int id;
        int inStorageSize;
        int outStorageSize;
        int numberChargingSpace;
        int * distances;
        int sender_id; 
        int numberOutPackages; //init 0
        int numberInPackages;//
        vector<bool> outpackageOccupied;
        vector<bool> inPackageOccupied;
        PackageInfo *incomingPackages;
        PackageInfo *outgoingPackages;
        sem_t *outgoingMutexes; // initialize with 1
        sem_t *incomingMutexes; // initialize with 1
        sem_t outgoingStorageContainsPackage; // initialize with 0
        sem_t canDeposit; // init with outstorageSize
        int receiverId;
        sem_t activeMutex;// init with 1
        sem_t incomingStorageEmpty; // init with num of incoming storage size;
        sem_t chargingSpaceEmpty; // init with num of charging spaces
        sem_t incomingStorageFull; // initialize with 0 initially there is no spot with the package
        sem_t chargingSpaceFull; // init with 0 initially there is no spot with the package
        sem_t inPackgNumberMutex; // init with 1
        sem_t outPackgNumberMutex; // init with 1
        priority_queue<pair<int,int>> neighbourHubs; // (distance, hub_id)
        priority_queue<pair<int,int>> droneRanges; // (currentRange, droneId)
        sem_t droneRangesMutex; // init with 1
        

};

class Receiver{
    public:
        int id;
        int timeBtwTwoPackages;
        int assignedHubId;
        bool stopped;
        sem_t stopMutex;

};

class Drone{
    public:
        int id;
        int speed;
        int currentHubId; //startingHubId
        int maxRange;
        int currentRange; // equal maxRange initially
        sem_t signalFromHubReceived; // init with 0
        PackageInfo deliveryPackage;
        bool signalFromCurrentHub; // if it is called by current its current hub
        long long chargingStart;
        int indexOfTakenPackage; // assign a value in hub thread when a drone is called to give a package
        int nearbyHubId; //Calling hub id
        bool stopped;
        sem_t stopMutex;

};



int numberSender, numberReceiver, numberHub, numberDrone;
int numberActiveHub;
sem_t numberSenderMutex, numberActiveHubMutex; // init with 1


Sender *senders;
Hub *hubs;
Receiver *receivers;
Drone *drones;

void *senderRoutine(void *p){
    int s_id= *((int *)p);
    Sender *sender = &senders[s_id];
    SenderInfo senderInfo;
    FillSenderInfo(&senderInfo, sender->id, sender->assignedHubId, sender->totalPackages, NULL);
    WriteOutput(&senderInfo, NULL, NULL, NULL, SENDER_CREATED);
    Hub *assignedHub = &hubs[sender->assignedHubId];
    Hub *receivingHub;
    int remainingPackages = sender->totalPackages;
    bool isDeposited; 
    PackageInfo depositedPackage;
    srand(time(0));
    int receivingHubId;
    while(remainingPackages>0){
        
        receivingHubId =rand()%numberHub +1; // SELECT RANDOMLY between 1 and numberHub+1 bc HubId s start from 1
        while (receivingHubId== assignedHub->id){
            receivingHubId =rand()%numberHub +1;
        }
        receivingHub = &hubs[receivingHubId];
        sem_wait(&assignedHub->canDeposit);
        FillPacketInfo(&depositedPackage, sender->id, assignedHub->id, receivingHub->receiverId, receivingHub->id);

        isDeposited = false;
        while (!isDeposited)
        {
            for(int i=0; i< assignedHub->outStorageSize;i++){
                if(assignedHub->outpackageOccupied[i]==false && sem_trywait(&assignedHub->outgoingMutexes[i])==0){
                    isDeposited = true;
                    assignedHub->outpackageOccupied[i] = true;
                    assignedHub->outgoingPackages[i] = depositedPackage;
                    sem_wait(&assignedHub->outPackgNumberMutex);
                    assignedHub->numberOutPackages++;
                    sem_post(&assignedHub->outPackgNumberMutex);
                    sem_post(&assignedHub->outgoingMutexes[i]);
                    
                    break;
                    

                }
            }
        }
        
        
        sem_post(&assignedHub->outgoingStorageContainsPackage);        
        
        FillSenderInfo(&senderInfo, sender->id, sender->assignedHubId, remainingPackages, &depositedPackage);
        WriteOutput(&senderInfo, NULL, NULL, NULL, SENDER_DEPOSITED);
        remainingPackages--;
        wait(UNIT_TIME*sender->timeBtwTwoPackages);




    }
    //sender stopped
    sem_wait(&numberSenderMutex);
    numberSender--;
    sem_post(&numberSenderMutex);
    FillSenderInfo(&senderInfo, sender->id, sender->assignedHubId, remainingPackages, NULL);
    WriteOutput(&senderInfo, NULL, NULL, NULL, SENDER_STOPPED);
    return nullptr;
}

void *receiverRoutine(void *p){
    int r_id= *((int *)p);
    Receiver *receiver = &receivers[r_id];
    Hub *assignedHub = &hubs[receiver->assignedHubId];
    ReceiverInfo receiverInfo;
    FillReceiverInfo(&receiverInfo, receiver->id, receiver->assignedHubId, NULL);
    WriteOutput(NULL, &receiverInfo, NULL, NULL, RECEIVER_CREATED);
    bool isPicked;
    PackageInfo pickedPackage;
    while(1){
        sem_wait(&assignedHub->incomingStorageFull); // check if there exists occupied slot in incoming storage
        sem_wait(&receiver->stopMutex);
        if(receiver->stopped){
            sem_post(&receiver->stopMutex);
            break;
        }
        sem_post(&receiver->stopMutex);
        isPicked = false;
        while(!isPicked){
            for(int i=0;i<assignedHub->inStorageSize;i++){
                if(assignedHub->inPackageOccupied[i]==true && sem_trywait(&assignedHub->incomingMutexes[i])==0){
                    isPicked = true;
                    pickedPackage = assignedHub->incomingPackages[i];
                    assignedHub->inPackageOccupied[i]=false;
                    sem_wait(&assignedHub->inPackgNumberMutex);
                    assignedHub->numberInPackages--;
                    sem_post(&assignedHub->inPackgNumberMutex);
                    FillPacketInfo(&pickedPackage, pickedPackage.sender_id, pickedPackage.sending_hub_id, pickedPackage.receiver_id, pickedPackage.receiving_hub_id);
                    FillReceiverInfo(&receiverInfo, receiver->id, receiver->assignedHubId, &pickedPackage);
                    WriteOutput(NULL, &receiverInfo, NULL, NULL, RECEIVER_PICKUP);
                    sem_post(&assignedHub->incomingMutexes[i]);
                    break;
                }
            }
            
        }
        sem_post(&assignedHub->incomingStorageEmpty); // since receiver took a package from incoming empty space increases
        wait(UNIT_TIME*receiver->timeBtwTwoPackages);


    }
    FillReceiverInfo(&receiverInfo, receiver->id, receiver->assignedHubId, NULL);
    WriteOutput(NULL, &receiverInfo, NULL, NULL, RECEIVER_STOPPED);

    return nullptr;


    
}


void *droneRoutine(void *p){
    int d_id= *((int *)p);
    PackageInfo deliveryPackageInfo;
    Drone * drone = &drones[d_id];
    Hub *currentHub;
    DroneInfo droneInfo;
    Hub *destinationHub;
    FillDroneInfo(&droneInfo, drone->id, drone->currentHubId, drone->currentRange, NULL, 0);
    WriteOutput(NULL, NULL, &droneInfo, NULL, DRONE_CREATED);
    bool spotReserved;
    int occupiedIndex, requiredRange, chargedRange;
    while(1){
        sem_wait(&drone->signalFromHubReceived);
        sem_wait(&drone->stopMutex);
        if(drone->stopped){
            sem_post(&drone->stopMutex);
            break;
        }
        sem_post(&drone->stopMutex);
        if(drone->signalFromCurrentHub){
            currentHub = &hubs[drone->currentHubId];
            destinationHub = &hubs[drone->deliveryPackage.receiving_hub_id];
            sem_wait(&destinationHub->incomingStorageEmpty); //check if there is any empty space in the incomingstorage
            sem_wait(&destinationHub->chargingSpaceEmpty); // check if there is any empty space in the charginspace
            // empty spaces found now reserve a spot in the incoming storage to keep packageinfos in hand
            spotReserved = false;
            while(!spotReserved){
                for(int i=0; i<  destinationHub->inStorageSize; i++){
                    if(destinationHub->inPackageOccupied[i]==false && sem_trywait(&destinationHub->incomingMutexes[i])== 0){
                        
                        destinationHub->inPackageOccupied[i]=true; //space occupied for the deliveryPackage
                        spotReserved = true;
                        occupiedIndex = i;
                        break;
                    }
                }
            }

            //WaitForRange()
            requiredRange = (currentHub->distances[destinationHub->id]/drone->speed);
            chargedRange = calculate_drone_charge((timeInMilliseconds()-drone->chargingStart), drone->currentRange, drone->maxRange);
            if(chargedRange>=requiredRange){
                //OK ready to go
            }
            else{
                wait(UNIT_TIME*(requiredRange-chargedRange));
                chargedRange = calculate_drone_charge((timeInMilliseconds()-drone->chargingStart), drone->currentRange, drone->maxRange);
            }
            // CurrentRange ← min(CurrentRange + ChargedRange, M aximumRange)
            drone->currentRange = min(chargedRange, drone->maxRange);

            //TakePackageFromHub

            FillPacketInfo(&deliveryPackageInfo, (drone->deliveryPackage).sender_id, (drone->deliveryPackage).sending_hub_id, (drone->deliveryPackage).receiver_id, (drone->deliveryPackage).receiving_hub_id);
            FillDroneInfo(&droneInfo, drone->id, drone->currentHubId, drone->currentRange, &deliveryPackageInfo, 0);
            WriteOutput(NULL, NULL, &droneInfo, NULL, DRONE_PICKUP);

            
            currentHub->outpackageOccupied[drone->indexOfTakenPackage] = false;
            sem_post(&currentHub->outgoingMutexes[drone->indexOfTakenPackage]); //package taken other threads can access this spot
            sem_post(&currentHub->canDeposit); // signal to the hub that an outgoing package has beeen picked  up
            sem_post(&currentHub->chargingSpaceEmpty); // one of the charging spaces became available            
            
          

            
            travel(currentHub->distances[destinationHub->id], drone->speed);
            drone->currentRange=drone->currentRange-(currentHub->distances[destinationHub->id]/drone->speed);
            

            

            //DropPackageToHub destinationHub 

            
            destinationHub->incomingPackages[occupiedIndex] = drone->deliveryPackage;
            sem_post(&destinationHub->incomingMutexes[occupiedIndex]); //package put safely now others can access it
            
            sem_wait(&destinationHub->inPackgNumberMutex);
            destinationHub->numberInPackages++;
            sem_post(&destinationHub->inPackgNumberMutex);
            //hub informed
            sem_post(&destinationHub->incomingStorageFull); // number of packages in the incoming storage increased
            
            // CurrentHub ← DestinationHub
            drone->currentHubId=destinationHub->id;
            drone->chargingStart = timeInMilliseconds();
            
            sem_wait(&destinationHub->droneRangesMutex);
            (destinationHub->droneRanges).push(make_pair(drone->currentRange, drone->id));
            sem_post(&destinationHub->droneRangesMutex);
            

            FillPacketInfo(&deliveryPackageInfo, (drone->deliveryPackage).sender_id, (drone->deliveryPackage).sending_hub_id, (drone->deliveryPackage).receiver_id, (drone->deliveryPackage).receiving_hub_id);
            FillDroneInfo(&droneInfo, drone->id, drone->currentHubId, drone->currentRange, &deliveryPackageInfo, 0);
            WriteOutput(NULL, NULL, &droneInfo, NULL, DRONE_DEPOSITED);

            
        }
        else{
            currentHub = &hubs[drone->currentHubId];
            destinationHub = &hubs[drone->nearbyHubId];
            // WaitAndReserveChargingSpace
            sem_wait(&destinationHub->chargingSpaceEmpty); // wait for the empty charging spot 
            //WaitForRange()
            requiredRange = (currentHub->distances[destinationHub->id]/drone->speed);
            chargedRange = calculate_drone_charge((timeInMilliseconds()-drone->chargingStart), drone->currentRange, drone->maxRange);
            if(chargedRange>=requiredRange){
                //OK ready to go
            }
            else{
                wait(UNIT_TIME*(requiredRange-chargedRange));
                chargedRange = calculate_drone_charge((timeInMilliseconds()-drone->chargingStart), drone->currentRange, drone->maxRange);
            }
            // CurrentRange ← min(CurrentRange + ChargedRange, M aximumRange)
            drone->currentRange = min(chargedRange, drone->maxRange);
            
            FillDroneInfo(&droneInfo, drone->id, drone->currentHubId, drone->currentRange, NULL, destinationHub->id);
            WriteOutput(NULL, NULL, &droneInfo, NULL, DRONE_GOING);
            sem_post(&currentHub->chargingSpaceEmpty); // one of the chargings spots becme empty in the current hub
            travel(currentHub->distances[destinationHub->id], drone->speed);
            drone->currentRange=drone->currentRange-(currentHub->distances[destinationHub->id]/drone->speed);
            // CurrentHub ← DestinationHub
            drone->currentHubId=destinationHub->id;

            sem_wait(&destinationHub->droneRangesMutex);
            drone->chargingStart = timeInMilliseconds();

            (destinationHub->droneRanges).push(make_pair(drone->currentRange, drone->id));
                        
            FillDroneInfo(&droneInfo, drone->id, drone->currentHubId, drone->currentRange, NULL, 0 );
            WriteOutput(NULL, NULL, &droneInfo, NULL, DRONE_ARRIVED);
            sem_post(&destinationHub->droneRangesMutex);

        }   
    }
    FillDroneInfo(&droneInfo, drone->id, drone->currentHubId, drone->currentRange, NULL, 0 );
    WriteOutput(NULL, NULL, &droneInfo, NULL, DRONE_STOPPED);
    return nullptr;
}

void *hubRoutine(void *p){
    int h_id= *(int *)p;
    Hub *hub = &hubs[h_id];
    HubInfo hubInfo;
    FillHubInfo(&hubInfo, h_id);
    WriteOutput(NULL, NULL, NULL, &hubInfo, HUB_CREATED);
    int assignedDroneId;
    Drone *assignedDrone;
    PackageInfo deliveryPackage;
    bool packageSelected,droneCalled, waiting=false;
    queue<pair<int,int>> temp;
    Hub * checkHub;
    Sender * sender = &senders[hub->sender_id]; 
    bool senderExists;
    while (1){
       
        
        someoneMaySendPackage:
        sem_wait(&hub->outPackgNumberMutex);
        if(hub->numberOutPackages<=0){
            sem_post(&hub->outPackgNumberMutex);        
            
            sem_wait(&numberSenderMutex);
            if(numberSender>0){
                sem_post(&numberSenderMutex);
                goto someoneMaySendPackage;
                //continue new packages can come
            }
            else{
                sem_post(&numberSenderMutex);
                while(sem_wait(&hub->inPackgNumberMutex)==0 && hub->numberInPackages>0){
                    sem_post(&hub->inPackgNumberMutex);
                    //wait for receiver picks all the incoming packages to quit
                }
                
                sem_post(&hub->inPackgNumberMutex);
                
                break;
        


            }
        }
        else{
            //char buffer [50];
            //sprintf(buffer, "numout>0 for hub id: ->%d", hub->id );
            //cout<<buffer<<endl; 
            sem_post(&hub->outPackgNumberMutex);
            goto start;
        }
            
        
        
        start:
        sem_wait(&hub->outPackgNumberMutex);
        if(hub->numberOutPackages<=0){
            sem_post(&hub->outPackgNumberMutex);
            goto someoneMaySendPackage;
        }
        sem_post(&hub->outPackgNumberMutex);
        //outpackExists:
        sem_wait(&hub->outgoingStorageContainsPackage);
        
        sem_wait(&hub->droneRangesMutex);
        if((hub->droneRanges).size()>0){
            waiting = false; 
            assignedDroneId = (hub->droneRanges).top().second;
            (hub->droneRanges).pop();
            sem_post(&hub->droneRangesMutex);
            assignedDrone= &drones[assignedDroneId];
            packageSelected = false;
            while(!packageSelected){
                for(int i=0; i< hub->outStorageSize;i++){
                    if(hub->outpackageOccupied[i]==true && sem_trywait(&hub->outgoingMutexes[i])==0){
                        deliveryPackage = hub->outgoingPackages[i];
                        assignedDrone->indexOfTakenPackage = i;
                        packageSelected=true;
                        break;
                    }
                }
            }
            assignedDrone->deliveryPackage = deliveryPackage;
            sem_wait(&hub->outPackgNumberMutex);
            hub->numberOutPackages--;
            sem_post(&hub->outPackgNumberMutex);
            
            assignedDrone->signalFromCurrentHub=true;
            sem_post(&assignedDrone->signalFromHubReceived); //signal for drone thread it has delivery package
        }
        else{
            sem_post(&hub->outgoingStorageContainsPackage);
            sem_post(&hub->droneRangesMutex);
            droneCalled=false;
            if(!waiting){
                while(!((hub->neighbourHubs).empty())){

                    checkHub = &hubs[(hub->neighbourHubs).top().second];
                    sem_wait(&checkHub->droneRangesMutex);
                    if((checkHub->droneRanges).size()){
                        assignedDrone = &drones[(checkHub->droneRanges).top().second]; 
                        checkHub->droneRanges.pop();
                        sem_post(&checkHub->droneRangesMutex);
                        assignedDrone->signalFromCurrentHub=false;
                        assignedDrone->nearbyHubId=hub->id;
                        sem_post(&assignedDrone->signalFromHubReceived);
                        droneCalled=true;
                        waiting = true;
                        break;
                    }
                    sem_post(&checkHub->droneRangesMutex);
                    temp.push((hub->neighbourHubs).top());
                    (hub->neighbourHubs).pop();
                    
                    

                }
                //gezilen hub listesini eski haline cevir
                while(!temp.empty()){
                    (hub->neighbourHubs).push(temp.front());
                    temp.pop();
                }
            }
            if(!droneCalled){
                long long waitStarted = timeInMilliseconds();
                while((timeInMilliseconds()-waitStarted)/UNIT_TIME <1){
                    sem_wait(&hub->droneRangesMutex);
                    if(hub->droneRanges.size()>0){
                        sem_post(&hub->droneRangesMutex);
                        goto start;    
                    }
                    sem_post(&hub->droneRangesMutex);
                }
               
                goto start;
            }

        
        }
    }
    
    
    sem_wait(&numberActiveHubMutex);
    numberActiveHub--;
    if(numberActiveHub==0){
        for(int j=0; j<numberDrone;j++){
            sem_post(&drones[j+1].signalFromHubReceived); // signal for drone to quit
            sem_wait(&drones[j+1].stopMutex);
            drones[j+1].stopped=true;
            sem_post(&drones[j+1].stopMutex);

        }
    }
    sem_post(&numberActiveHubMutex); 

    sem_post(&hub->incomingStorageFull); //signal for receiver to quit
    sem_wait(&receivers[hub->receiverId].stopMutex);
    receivers[hub->receiverId].stopped=true;
    sem_post(&receivers[hub->receiverId].stopMutex);
    
    FillHubInfo(&hubInfo,hub->id);
    WriteOutput(NULL,NULL,NULL,&hubInfo,HUB_STOPPED);
    

    return nullptr;
    
    
}


int main(){
    sem_init(&numberSenderMutex, 0, 1);
    sem_init(&numberActiveHubMutex,0,1);
    cin>>numberHub;
    numberReceiver=numberHub;
    numberSender = numberHub;
    numberActiveHub=numberHub;
    //hubs created 
    hubs = new Hub[numberHub+1];
    Hub *hub;
    for(int i=0; i< numberHub;i++){
        hub = &hubs[i+1]; // ids start from 1
        hub->id = i+1;
        cin >> hub->inStorageSize;
        cin >> hub->outStorageSize;
        cin >> hub->numberChargingSpace;
        hub->distances = new int[numberHub+1];
        for(int j=0;j<numberHub;j++){
            cin>>hub->distances[j+1];
            if(j+1 != hub->id){
                hub->neighbourHubs.push(make_pair(hub->distances[j+1],j+1));
            }
            
        }
        hub->numberOutPackages=0;
        hub->numberInPackages=0;
        hub->outpackageOccupied=vector<bool>(hub->outStorageSize,false);
        hub->inPackageOccupied=vector<bool>(hub->inStorageSize,false);
        hub->incomingPackages = new PackageInfo[hub->inStorageSize];
        hub->outgoingPackages = new PackageInfo[hub->outStorageSize]; 
        hub->outgoingMutexes = new sem_t[hub->outStorageSize];
        hub->incomingMutexes = new sem_t[hub->inStorageSize];
        sem_init(&hub->outgoingStorageContainsPackage, 0, 0);
        sem_init(&hub->canDeposit, 0, hub->outStorageSize);
        sem_init(&hub->activeMutex,0,1);
        sem_init(&hub->incomingStorageEmpty, 0, hub->inStorageSize);
        sem_init(&hub->chargingSpaceEmpty, 0, hub->numberChargingSpace);
        sem_init(&hub->incomingStorageFull, 0, 0);
        sem_init(&hub->chargingSpaceFull, 0, 0);
        sem_init(&hub->inPackgNumberMutex,0,1);
        sem_init(&hub->outPackgNumberMutex,0,1);
        sem_init(&hub->droneRangesMutex,0,1);
        for(int k=0; k < hub->outStorageSize;k++){
            sem_init(&hub->outgoingMutexes[k],0,1);
        }
        for(int k=0; k < hub->inStorageSize;k++){
            sem_init(&hub->incomingMutexes[k],0,1);
        }

        



    }
    
    //senders created 
    senders = new Sender[numberHub+1];
    Sender *sender;
    for(int i=0;i<numberSender;i++){
        sender = &senders[i+1]; 
        sender->id = i+1;
        cin >> sender->timeBtwTwoPackages;
        cin >> sender->assignedHubId;
        cin >> sender->totalPackages;
        hubs[sender->assignedHubId].sender_id = sender->id;

    }

    //receivers cretaed
    receivers = new Receiver[numberHub+1];
    Receiver *receiver;
    for(int i=0;i<numberReceiver;i++){
        receiver = &receivers[i+1];
        receiver->id = i+1;
        cin>> receiver->timeBtwTwoPackages;
        cin>> receiver->assignedHubId;
        sem_init(&receiver->stopMutex, 0, 1);

        receiver->stopped=false;
        hubs[receiver->assignedHubId].receiverId=receiver->id;


    }
    
    cin>> numberDrone;
    
    //drones created and pushed
    drones = new Drone[numberDrone+1];
    Drone *drone;
    for(int i=0;i<numberDrone;i++){
        drone = &drones[i+1];
        drone->id=i+1;
        cin>> drone->speed;
        cin >> drone->currentHubId;
        cin >> drone->maxRange;
        drone->currentRange=drone->maxRange;
        sem_post(&hubs[drone->currentHubId].chargingSpaceFull);
        sem_wait(&hubs[drone->currentHubId].chargingSpaceEmpty);
        hubs[drone->currentHubId].droneRanges.push(make_pair(drone->currentRange, drone->id));
        sem_init(&drone->signalFromHubReceived,0,0);
        drone->stopped=false;
        sem_init(&drone->stopMutex,0,1);
    }
    

    pthread_t senderThreads[numberSender];
    pthread_t receiverThreads[numberReceiver];
    pthread_t hubThreads[numberHub];
    pthread_t droneThreads[numberDrone];

    //threads
    InitWriteOutput();
    int *ids;
    ids = new int[numberHub];
    for(int i=1;i<numberHub+1;i++){
        ids[i]=i;
        pthread_create(&hubThreads[i-1], NULL, hubRoutine, &ids[i] );
    }
    for(int i=1;i<numberHub+1;i++){
        ids[i]=i;
        pthread_create(&senderThreads[i-1], NULL, senderRoutine, (void *)&ids[i] );
    }
    for(int i=1;i<numberReceiver+1;i++){
        ids[i]=i;
        pthread_create(&receiverThreads[i-1], NULL, receiverRoutine, (void *)&ids[i] );
    }
    int *droneIds;
    droneIds = new int[numberDrone];
    for(int i=1;i<numberDrone+1;i++){
        droneIds[i]=i;
        pthread_create(&droneThreads[i-1], NULL, droneRoutine, (void *)&droneIds[i] );
    }
    for(int i=0;i<numberHub;i++){
        pthread_join(hubThreads[i],NULL);
        pthread_join(senderThreads[i],NULL);
        pthread_join(receiverThreads[i],NULL);

    }
    for(int i=0;i<numberDrone;i++){
        pthread_join(droneThreads[i],NULL);

    }

    for(int i=0;i<numberHub;i++){
        delete[] hubs[i+1].distances;
        delete[] hubs[i+1].incomingPackages;
        delete[] hubs[i+1].outgoingPackages;
        delete[] hubs[i+1].outgoingMutexes;
        delete[] hubs[i+1].incomingMutexes;

    }

    delete[] hubs;
    delete[] senders;
    delete[] receivers;
    delete[] drones;
    delete[] ids;
    delete[] droneIds;


}