package vmsim;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Random;
import java.util.Scanner;

public class vmsim1 {

    public static void main(String[] args) throws FileNotFoundException {
        
        if(args.length<5){
            System.out.println("args: n <numframes> -a <opt|clock|nru|work> [-r <refresh>] [-t <tau>] <tracefile>");
            System.exit(0);
        }
        
        if(args.length == 5 && args[0].equals("-n") && args[2].equals("-a")){
            int numFrames = Integer.parseInt(args[1]);
            if(args[3].equals("opt")){
                opt(numFrames,args[4]);
            }else if(args[3].equals("clock")){
                clock(numFrames,args[4]);
            }
        }else if(args.length == 7 && args[0].equals("-n") && args[2].equals("-a") && args[4].equals("-r")){
            int numFrames = Integer.parseInt(args[1]);
            int r = Integer.parseInt(args[5]);            
            nru(numFrames,r,args[6]);
        }else if(args.length == 9 && args[0].equals("-n") && args[3].equals("work") && args[2].equals("-a") && args[4].equals("-r") && args[6].equals("-t")){
            int numFrames = Integer.parseInt(args[1]);
            int r = Integer.parseInt(args[5]);  
            int t = Integer.parseInt(args[7]);
            //Do Working Set Page Replacement
            work(numFrames,r,t,args[8]);
        }else{
            System.out.println("args: n <numframes> -a <opt|clock|nru|work> [-r <refresh>] [-t <tau>] <tracefile>");
            System.exit(0);
        }
    
    }

    //Optimal Page Replacement
    public static void opt(int numFrames,String traceFile) throws FileNotFoundException{
        
        File file = new File(traceFile);
        Scanner trace;
        trace = new Scanner(file);
        ArrayList<Long> pages = new ArrayList<>();    //Goes through every page in VA and adds it to this list
        ArrayList<LinkedList<Integer>> nextRef = new ArrayList<>();  //Holds every future time the correspoding memory location is referenced
        ArrayList<Long> memory = new ArrayList<>();                  //holds the pages currently in memory                
        int[] d = new int[numFrames];                   //if each corresponding page has been written
        int accesses = 0;
        int faults = 0;
        int writes = 0;
        int pageSize = 32000;
        Long frame;

        while(trace.hasNext()){//Pre Search through the file and find look at all the memory references for opt
            frame = Long.parseLong(trace.next(), 16)/pageSize;
            if(!pages.contains(frame)){//If this is the first time an address is acessed we need to add it to the list of pages;
                pages.add(frame);
                LinkedList<Integer> a = new LinkedList<>();
                a.add(accesses);
                nextRef.add(a);
            }else{//If this addrss is already in the array, find it in nextRef and add to its linked list
                int loc = pages.indexOf(frame);
                nextRef.get(loc).add(accesses);
            }       
            trace.nextLine();
            accesses++;
        } 


        int free = numFrames;//Run Opt
        boolean w;
        accesses = 0;        
        trace = new Scanner(file);        
        while(trace.hasNext()){          
            frame = Long.parseLong(trace.next(), 16)/pageSize;
            w = trace.next().equals("W");
            nextRef.get(pages.indexOf(frame)).removeFirst();//removes the current page from its linked list            
            
            if(memory.contains(frame)){
                //System.out.println("Hit");              
                if(w){
                    d[memory.indexOf(frame)] = 1;
                }
            }else if(free>0){//adds new pages to memory (doesnt evict anything
                free--;
                faults++;           
                //System.out.println("Page Fault - No Eviction");    
                memory.add(frame);
                if(w){
                    d[memory.indexOf(frame)] = 1;
                }else{
                    d[memory.indexOf(frame)] = 0;
                }  
            }else{
                faults++;
                Integer farRef = 0;
                int evict = 0;//The page that we will evict              
                for(int i=0; i<memory.size();i++){//Find a page to evict from memory
                    int thisPage = pages.indexOf(memory.get(i)); 
                    if(nextRef.get(thisPage).size()==0){//There are no more references so we can remove it and place the new page here
                        nextRef.remove(thisPage);
                        pages.remove(thisPage);
                        evict = i;
                        break;
                    }else if(nextRef.get(thisPage).getFirst()>farRef){
                        farRef = nextRef.get(thisPage).getFirst();
                        evict = i;
                    }
                }
                if(d[evict] == 1){
                    writes++;
                    //System.out.println("Page Fault - Evict Dirty");
                }else{
                    //System.out.println("Page Fault - Evict Clean");
                }
                
                memory.set(evict,frame);//add the new memory to the evicted location
                if(w){
                    d[evict] = 1;
                }else{
                    d[evict] = 0;
                }    
            }
            accesses++;
        }
        
        //Print Results
        System.out.println("Opt");
        System.out.println("Number Of Frames:\t" + numFrames);
        System.out.println("Total Memory Accesses:\t" + accesses);
        System.out.println("Total Page Faults:\t" + faults);
        System.out.println("Total Writes To Disk:\t" + writes);
    }
    
    //Not Recently Used Page Replacement
    public static void nru(int numFrames,int refresh,String traceFile) throws FileNotFoundException{
        File file = new File(traceFile);
        Scanner trace = new Scanner(file);
        int accesses = 0;
        int faults = 0;
        int writes = 0;                                 
        int pageSize = 32000;                           //number of address in each page
        ArrayList<Long> memory = new ArrayList<>();     //RAM
        int[] d = new int[numFrames];                   //Holds Dirty Bit
        int[] r = new int[numFrames];                   //Holds Referenced Bit
        boolean w;                                      //Check if read or write
        Long frame;                                     //Holds frame of each address
        int free = numFrames;                           //Number of open frames available
        int nextRefresh = refresh;                      //Updates after each refresh
        
        while(trace.hasNext()){
            if(accesses==nextRefresh){                              //refresh each referenced bit to 0 if it's time
                for(int i=0;i<r.length;i++){
                    r[i] = 0;
                }
                nextRefresh += refresh;
            }
            accesses++;                                             //keeps track of line we are on as well as number of accesses     
            frame = Long.parseLong(trace.next(), 16)/pageSize;      //frame/pagesize = frame
            w = trace.next().equals("W");                           
            if(memory.contains(frame)){                             //if memory already has frame there is no pagefault just update r and d
                //System.out.println("hit");
                r[memory.indexOf(frame)] = 1;
                if(w){
                    d[memory.indexOf(frame)] = 1;
                }
                
            }else if(free>0){                                       //if there is free space in memory add the frame there
                int loc = numFrames-free;                
                free--;
                faults++;
                //System.out.println("Page Fault - No Eviction");
                memory.add(frame);
                r[loc] = 1;
                if(w){
                    d[loc] = 1;
                }else{
                    d[loc] = 0;
                }
            }else{                                                  //Need to evict a page
                faults++;
                int bestFound = 0;                                  //The best combonation of r&d found so far
                for(int i=0;i<memory.size();i++){
                    if(r[i]==0 && d[i]==0 && bestFound<4){          //find the lowest class in memory
                        bestFound = 4;
                    }else if(r[i]==0 && d[i]==1 && bestFound<3){    //of r=0 and d=1
                        bestFound = 3;
                    }else if(r[i]==1 && d[i]==0 && bestFound<2){    //if r=0 and d=0
                        bestFound = 2;
                    }else if(r[i]==1 && d[i]==1 && bestFound<1){    //if r=1 and d=1
                        bestFound = 1;
                    }
                }
                ArrayList<Integer> lowClass = new ArrayList<>();//add every frame that is of the lowest class and one will be randomly evicted
                for(int i=0;i<memory.size();i++){
                    if(r[i]==0 && d[i]==0 && bestFound==4){
                        lowClass.add(i);
                    }else if(r[i]==0 && d[i]==1 && bestFound==3){    //of r=0 and d=1
                        lowClass.add(i);
                    }else if(r[i]==1 && d[i]==0 && bestFound==2){    //if r=0 and d=0
                        lowClass.add(i);
                    }else if(r[i]==1 && d[i]==1 && bestFound==1){    //if r=1 and d=1
                        lowClass.add(i);
                    }
                }
                Random rand = new Random();
                int evict = lowClass.get(rand.nextInt(lowClass.size()));//chose a random frame from the lowest class and evict it
                if(d[evict] == 1){//Write dirty page to memory
                    //System.out.println("Page Fault, Evict Dirty");
                    writes++;
                }else{
                    //System.out.println("Page Fault, Evict Clean");
                }
                memory.set(evict,frame);//add new frame to location old frame was evicted from
                r[evict] = 1;//set new r bit
                if(w){//set new d bit
                    d[evict] = 1;
                }else{
                    d[evict] = 0;
                }
            }
        }
					
        //Print Results
        System.out.println("NRU");
        System.out.println("Number Of Frames:\t" + numFrames);
        System.out.println("Total Memory Accesses:\t" + accesses);
        System.out.println("Total Page Faults:\t" + faults);
        System.out.println("Total Writes To Disk:\t" + writes);   
    }
    
    //Second Chance Clock Algorithm
    public static void clock(int numFrames,String traceFile) throws FileNotFoundException{
        File file = new File(traceFile);
        Scanner trace;
        trace = new Scanner(file);
        ArrayList<Long> memory = new ArrayList<>();     //RAM
        int[] d = new int[numFrames];                   //dirty bit
        int[] r = new int[numFrames];                   //referenced bit
        int pageSize = 32000;                           //number of addresses in each frame
        int accesses = 0;                               
        int faults = 0;
        int writes = 0;
        Long frame;
	boolean w;                                      //Check if read or write
        int clockIndex = 0;                             //Oldest program in memory
        int open = numFrames;                           //Number of open frames available		
        
        while(trace.hasNext()){
            accesses++;//number of accesses as well as current line
            frame = Long.parseLong(trace.next(), 16)/pageSize;//determine which page current address is
            w = trace.next().equals("W");
            if(memory.contains(frame)){//if frame is in ram then we found it, update d bit
                //System.out.println("hit");
                if(w){
                    d[memory.indexOf(frame)] = 1;
                }				
            }else if(open>0){//if there are available space in memory
                faults++;
                //System.out.println("Page Fault - No Eviction");
                int loc = numFrames-open;
                memory.add(frame);
                open--;
                r[loc] = 1;
                if(w){
                    d[loc] = 1;
                }else{
                    d[loc] = 0;
                }
            }else{//we need to make an eviction
                faults++;
                boolean foundEvict = false;//found an unreferenced page to evict
                int evict = 0;//location of that page
                while(!foundEvict){
                    if(r[clockIndex]==0){//if the r it is 0, evict this page
                        evict = clockIndex;
                        if(clockIndex+1==r.length){
                            clockIndex = 0;
                        }else{
                            clockIndex++;
                        }
                        
                        foundEvict = true;
                    }else{//set the r bit to 0 and check the next frame
                        r[clockIndex]=0;
                        if(clockIndex+1==r.length){
                            clockIndex = 0;
                        }else{
                            clockIndex++;
                        }
                    }
                }
                if(d[evict] == 1){//if dirty, write to memory
                    //System.out.println("Page Fault, Evict Dirty");
                    writes++;
                }else{
                    //System.out.println("Page Fault, Evict Clean");
                }
                memory.set(evict,frame);//add new page in frame of evicted memory
                r[evict] = 1;//r bit always starts a 1
                if(w){//check for a write
                    d[evict] = 1;
                }else{
                    d[evict] = 0;
                }                
                
            }
        }
		
        //Print Results
        System.out.println("Clock");
        System.out.println("Number Of Frames:\t" + numFrames);
        System.out.println("Total Memory Accesses:\t" + accesses);
        System.out.println("Total Page Faults:\t" + faults);
        System.out.println("Total Writes To Disk:\t" + writes);
    }    
    
    //Working Set Page Replacement
    public static void work(int numFrames,int refresh,int t,String traceFile) throws FileNotFoundException{
        File file = new File(traceFile);
        Scanner trace;
        trace = new Scanner(file);
        int[] d = new int[numFrames];                   //dirty bit
        int[] r = new int[numFrames];                   //referenced bit
        ArrayList<Long> memory = new ArrayList<>();     //RAM        
        int accesses = 0;
        int faults = 0;
        int writes = 0;
        int pageSize = 32000;                           //number of addresses in each page
        Long frame;
	boolean w;                                      //Check if read or write        
	int[] lastTime = new int[numFrames];            //time of last use
        int[] v = new int[numFrames];                   //valid bit
        int free = numFrames;                           //number of invalid frames
        System.out.println(free);
        int index = 0;                                  //current location of clock
        int nextRefresh = refresh;                      //next time we need a refresh
        long x = 0;                                     //start all frames at 0
        for(int i=0;i<numFrames;i++){
            memory.add(x);
        }
        int time = 0;//keeps track of time by line number
        
        while(trace.hasNext()){
            if(accesses==nextRefresh){//if we need to refresh
                for(int i=0;i<r.length;i++){//reset referenced bit every refresh
                    if(r[i]==1){
                        lastTime[i] = time;
                    }
                    r[i] = 0;
                    nextRefresh += refresh;
                }
            }
            accesses++;//keeps track of accesses
            time = accesses;//also used for out current time
            frame = Long.parseLong(trace.next(), 16)/pageSize;//find what page an address is in
            w = trace.next().equals("W");
            if(memory.contains(frame)){//if page is already in memory
                //System.out.println("Hit");
                if(w){
                    d[memory.indexOf(frame)] = 1;//update dirty bit
                }
                r[memory.indexOf(frame)] = 1;//update to referenced bit
            }else if(free>0){//add it to an open space                
                free--;
                faults++;
                boolean found = false;
                int j = 0;
                //System.out.println("Page Fault - No Eviction");
                while(!found){
                    if(v[j] == 0){//if the frame is free
                        v[j] = 1;//its now being used
                        r[j] = 1;//start r bit at 1
                        lastTime[j] = time;
                        if(w){//update d bit
                            d[j] = 1;
                        }else{
                            d[j] = 0;
                        }
                        memory.set(j,frame);//put page into this frame
                        found = true;
                    }
                    j++;
                }
            }else{
                int oldestTime = 0;//search through list using algorithm
                int oldest = 0;//keeps track of oldest incase there are no pages to evict
                if(index==memory.size()){//keeps queue circular
                    index=0;
                }                
                int start = index;
                boolean first = true;
                boolean found = false;//if we evicted something in while loop
                boolean done = false;
                while(!done){//go until we have been through each frame or find something to evict
                    if(index==memory.size()){//keeps queue circular
                        index=0;
                    }
                    if(index==start&&!first){//have been through the entire pagetable and there are no unreferenced clean pages, remove oldest
                        done = true;
                    }
                    if(first){
                        oldestTime = lastTime[index];
                        first = false;
                    }
                    if(lastTime[index]<oldestTime&&v[index]==1){//keeps track of the oldest one incase we eed to use it
                        oldest = index;
                        oldestTime = lastTime[index];
                    }
                    if(r[index]==1&&v[index]==1){
                        lastTime[index] = time;//if a frame has been referenced set the last time it's been referenced to now 
                    }else if(v[index] == 1){
                        int age = time-lastTime[index];//calculate age
                        if(d[index]==0){//we found a clean page, evict it
                            faults++;
                            //System.out.println("Page Fault - Evict Clean");
                            r[index] = 1;
                            lastTime[index] = time;                            
                            if(w){
                                d[index] = 1;
                            }else{
                                d[index] = 0;
                            }
                            memory.set(index,frame);
                            found = true;
                            done = true; 
                        }else{
                            if(age>t){//evict old and dirty pages as we see them                                
                                free++;
                                writes++;
                                //System.out.println("Evict Dirty");
                                v[index] = 0;               
                                long reset = -1;
                                memory.set(index,reset);
                            }
                        }
                    }
                    index++;
                }
                if(!found){//if no clean pages to evict were found, evict the oldest page
                    faults++;
                    //System.out.print("Page Fault - ");
                    if(d[oldest]==0){
                        //System.out.println("Evict Clean");
                    }else{
                        writes++;
                        //System.out.println("Evict Dirty");
                    }
                    r[oldest] = 1;
                    lastTime[oldest] = time;
                    if(w){
                        d[oldest] = 1;
                    }else{
                        d[oldest] = 0;
                    }
                    memory.set(oldest,frame);               
                }
            }
        }	
        //Print Results
        System.out.println("Work");
        System.out.println("Number Of Frames:\t" + numFrames);
        System.out.println("Total Memory Accesses:\t" + accesses);
        System.out.println("Total Page Faults:\t" + faults);
        System.out.println("Total Writes To Disk:\t" + writes);
    }    
}