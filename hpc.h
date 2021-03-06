#include <cstdio>
#include <limits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <random>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>
#else
  #define omp_get_thread_num() 0
  #define omp_get_max_threads() 1
#endif
using namespace std;
#include <limits>
const double epsilon = 1.0e-15;
const double bignum = 1.0;
const double threshold = 1.0e-10;
const unsigned int max_unsigned_int_size = std::numeric_limits<unsigned int>::max();

// ofstream with higher precision to avoid truncation errors
struct my_ofstream : ofstream {
  explicit my_ofstream(streamsize prec = 15)
  {
    this->precision(prec);
  }
};


template <class T>
inline string to_string (const T& t){
	stringstream ss;
	ss << t;
	return ss.str();
}

struct pairhash {
public:
  template <typename T, typename U>
  size_t operator()(const pair<T, U> &x) const
  {
    return x.first*31 + x.second;
  }
};

unsigned int convert_div_to_uintdiv(double div)
{ 
  return static_cast<unsigned int>(max_unsigned_int_size*div);
}

double convert_uintdiv_to_div(unsigned int uintdiv)
{ 
  return static_cast<double>(1.0*uintdiv/max_unsigned_int_size);
}

// // Identical hashes for T,U and U,T, but that will never happen since T,U are ordered
// struct pairhash {
// public:
//   template <typename T, typename U>
//   size_t operator()(const pair<T, U> &x) const
//   {
//     return hash<T>()(x.first) ^ hash<U>()(x.second);
//   }
// };

class Partition{
public:
	Partition();
	Partition(int partitionid, int Nnodes);
	int partitionId;
	vector<int> assignments;
	unordered_map<int,int> clusterSizes;
	double minDist = bignum;
	Partition *minCenterPartition;
};

Partition::Partition(){
};

Partition::Partition(int partitionid, int Nnodes){
	partitionId = partitionid;
	assignments = vector<int>(Nnodes);
}

typedef multimap< double, vector<Partition *>, greater<double> > SortedClusters;

class Clusters{
public:

	Clusters();
	unsigned int maxClusterSize = 0;
	double sumMaxDist = 0.0;
	SortedClusters sortedClusters;
};

Clusters::Clusters(){
};


class Partitions{
private:

	double wpJaccardDist(int partitionId1, int partitionId2);
	double wpJaccardDist(Partition *partition1, Partition *partition2);
	double calcMaxDist(vector<Partition *> &partitionPtrs);
	double calcMaxDist(vector<Partition *> &partition1Ptrs,vector<Partition *> &partition2Ptrs);

	void splitCluster(Clusters &clusters);
	void mergeClusters(Clusters &clusters);

	// double maxJaccardDiv(Cluster &clusterId1, Cluster &clusterId2);
	// void findCenters(Medoids &medoids);
	// void findClusters(Medoids &medoids);
	// double updateCenters(unordered_map<int,pair<double,vector<LocalStateNode> > > &newMedoids);
	
	vector<Partition> partitions;
	int Nskiplines = 0;
	int Nnodes = 0;
	int Npartitions = 0;

	int randInt(int from, int to);
	double randDouble(double to);

	string inFileName;
	string outFileName;
	int Nattempts = 1;
	int NdistAttempts = 1;
	double distThreshold = 0.2;
	double splitDistThreshold = 0.2;
	vector<mt19937> mtRands;
	ifstream ifs;
  string line;
	Clusters bestClusters;

	unsigned int NfinalClu;
	unsigned int NsplitClu;
	// unordered_map<pair<int,int>,double,pairhash> cachedWJSdiv;

public:
	Partitions(string inFileName,string outFileName,int Nskiplines,double distThreshold,double splitDistThreshold,unsigned int NsplitClu,int Nattempts,int NdistAttempts,int seed); 
	void readPartitionsFile();
	void clusterPartitions();
	void printClusters();

};

Partitions::Partitions(string inFileName,string outFileName,int Nskiplines,double distThreshold,double splitDistThreshold,unsigned int NsplitClu,int Nattempts,int NdistAttempts,int seed){
	this->Nskiplines = Nskiplines;
	this->distThreshold = distThreshold;
	this->splitDistThreshold = splitDistThreshold;
	this->NfinalClu = NfinalClu;
	this->NsplitClu = NsplitClu;
	this->Nattempts = Nattempts;
	this->NdistAttempts = NdistAttempts;
	this->inFileName = inFileName;
	this->outFileName = outFileName;

	int threads = max(1, omp_get_max_threads());

	for(int i = 0; i < threads; i++){
    mtRands.push_back(mt19937(seed+1));
  }

	// Open state network for building state node to physical node mapping
	ifs.open(inFileName.c_str());
	if(!ifs){
		cout << "failed to open \"" << inFileName << "\" exiting..." << endl;
		exit(-1);
	}
	readPartitionsFile();
	ifs.close();

	// for(int i=0;i<Npartitions;i++){
	// 	for(int j=i+1;j<Npartitions;j++){
	// 		cout << i << " " << j << " " << wpJaccardDist(i,j) << endl;
	// 	}
	// }

}

int Partitions::randInt(int from, int to){

	uniform_int_distribution<int> rInt(from,to);
	return rInt(mtRands[omp_get_thread_num()]);

}

double Partitions::randDouble(double to){

	uniform_real_distribution<double> rDouble(0.0,to);
	return rDouble(mtRands[omp_get_thread_num()]);

}

double Partitions::wpJaccardDist(int partitionId1, int partitionId2){


	unordered_map<pair<int,int>,int,pairhash> jointM;
	for(int k=0;k<Nnodes;k++){
	  jointM[make_pair(partitions[partitionId1].assignments[k],partitions[partitionId2].assignments[k])]++;
	}
	double sim = 0.0;
	for(unordered_map<pair<int,int>,int,pairhash>::iterator it = jointM.begin(); it != jointM.end(); it++){
	  int Ncommon = it->second;
	  int Ntotal = partitions[partitionId1].clusterSizes[it->first.first] + partitions[partitionId2].clusterSizes[it->first.second] - Ncommon;
	  sim += 1.0*Ncommon*Ncommon/(Nnodes*Ntotal);
	}
     
	return 1.0 - sim;

}

double Partitions::wpJaccardDist(Partition *partition1, Partition *partition2){


	unordered_map<pair<int,int>,int,pairhash> jointM;
	for(int k=0;k<Nnodes;k++){
	  jointM[make_pair(partition1->assignments[k],partition2->assignments[k])]++;
	}
	double sim = 0.0;
	for(unordered_map<pair<int,int>,int,pairhash>::iterator it = jointM.begin(); it != jointM.end(); it++){
	  int Ncommon = it->second;
	  int Ntotal = partition1->clusterSizes[it->first.first] + partition2->clusterSizes[it->first.second] - Ncommon;
	  sim += 1.0*Ncommon*Ncommon/(Nnodes*Ntotal);
	}
     
	return 1.0 - sim;

}

double Partitions::calcMaxDist(vector<Partition *> &partitionPtrs){

	#ifdef _OPENMP
	// Initiate locks to keep track of best solutions
	omp_lock_t lock;
  omp_init_lock(&lock);
	#endif

	int NClusterPartitions = partitionPtrs.size();
	double maxDist = 0.0;
	
	for(int attempts=0;attempts < NdistAttempts; attempts++){
		
		int randPartitionId = randInt(0,NClusterPartitions-1);
		int maxDistIdWithRandPartitionId = randPartitionId;
		double maxDistWithRandPartitionId = 0.0;
		
		int Nsteps = 2;
		int step = 0;
		while(step < Nsteps){
			randPartitionId = maxDistIdWithRandPartitionId;
			maxDistIdWithRandPartitionId = randPartitionId;
			maxDistWithRandPartitionId = 0.0;
			#pragma omp parallel for
			for(int i=0;i<NClusterPartitions;i++){
				if(i != randPartitionId){
					double dist = wpJaccardDist(partitionPtrs[randPartitionId],partitionPtrs[i]);
					#ifdef _OPENMP
					omp_set_lock(&lock);
					#endif					
					if(dist > maxDistWithRandPartitionId){									
						maxDistWithRandPartitionId = dist; // Max in round
						maxDistIdWithRandPartitionId = i;
						maxDist = max(maxDist,dist); // Max overall
					}
					#ifdef _OPENMP
					omp_unset_lock(&lock);
					#endif
				}
			}
			step++;
		}

	}
	
	return maxDist;
}

double Partitions::calcMaxDist(vector<Partition *> &partition1Ptrs,vector<Partition *> &partition2Ptrs){

	#ifdef _OPENMP
	// Initiate locks to keep track of best solutions
	omp_lock_t lock;
  omp_init_lock(&lock);
	#endif

	int NClusterPartitions1 = partition1Ptrs.size();
	int NClusterPartitions2 = partition2Ptrs.size();
	int NClusterPartitions = NClusterPartitions1 + NClusterPartitions2;

	double maxDist = 0.0;
	
	for(int attempts=0;attempts < NdistAttempts; attempts++){
		
		int randPartitionId = randInt(0,NClusterPartitions-1);
		int maxDistIdWithRandPartitionId = randPartitionId;
		double maxDistWithRandPartitionId = 0.0;
		
		int Nsteps = 2;
		int step = 0;
		while(step < Nsteps){
			randPartitionId = maxDistIdWithRandPartitionId;
			maxDistIdWithRandPartitionId = randPartitionId;
			maxDistWithRandPartitionId = 0.0;
			Partition *partitionPtr1 = (randPartitionId < NClusterPartitions1) ? partition1Ptrs[randPartitionId] : partition2Ptrs[randPartitionId-NClusterPartitions1];
			#pragma omp parallel for
			for(int i=0;i<NClusterPartitions;i++){
				if(i != randPartitionId){
					Partition *partitionPtr2 = (i < NClusterPartitions1) ? partition1Ptrs[i] : partition2Ptrs[i-NClusterPartitions1];;
					double dist = wpJaccardDist(partitionPtr1,partitionPtr2);
					#ifdef _OPENMP
					omp_set_lock(&lock);
					#endif					
					if(dist > maxDistWithRandPartitionId){									
						maxDistWithRandPartitionId = dist; // Max in round
						maxDistIdWithRandPartitionId = i;
						maxDist = max(maxDist,dist); // Max overall
					}
					#ifdef _OPENMP
					omp_unset_lock(&lock);
					#endif					
				}
			}
			step++;
		}

	}
	
	return maxDist;
}

void Partitions::clusterPartitions(){

	cout << "Clustering partitions:" << endl;

	// To keep track of best solutions
	int bestNClusters = Npartitions;
	double bestSumMaxDist = bignum*Npartitions;

	vector<Partition*> partitionPtrs = vector<Partition*>(Npartitions);
	for(int i=0;i<Npartitions;i++)
		partitionPtrs[i] = &partitions[i];

  	for(int attempt=0;attempt<Nattempts;attempt++){

  		double maxDist = calcMaxDist(partitionPtrs);
		Clusters clusters;
		clusters.maxClusterSize = Npartitions;
		clusters.sumMaxDist = maxDist;
		clusters.sortedClusters.insert(make_pair(maxDist,partitionPtrs));
		
		cout << "-->Attempt " << attempt+1 << "/" << Nattempts << ": First dividing " << Npartitions << " partitions..." << flush;
		splitCluster(clusters);
		cout << "into " << clusters.sortedClusters.size() << " clusters and then merging..." << flush;
		mergeClusters(clusters);
		double attemptNClusters = clusters.sortedClusters.size();
		double attemptSumMaxDist = clusters.sumMaxDist;
		cout << "into " << attemptNClusters << " clusters with maximum distance " << clusters.sortedClusters.begin()->first << ", average maximum distance " << attemptSumMaxDist/attemptNClusters << ", and maximum cluster size " << clusters.maxClusterSize << ".";

		// Update best solution

		if( (attemptNClusters < bestNClusters) || ((attemptNClusters == bestNClusters) && (attemptSumMaxDist < bestSumMaxDist)) ){
			bestNClusters = attemptNClusters;
			bestSumMaxDist = attemptSumMaxDist;
			bestClusters = move(clusters);
			// bestClusters = clusters;

			cout << " New best solution!";
		}
		cout << endl;
			
	} // end of for loop

}

void Partitions::mergeClusters(Clusters &clusters){

	unordered_map<int,SortedClusters::iterator> cluster_its;
	unordered_map<int,set<pair<unsigned int,int> > > sortedDists;
	multimap<unsigned int,int> minDists;

	// Initiate priority queues
	int clusterId1 = 0;
	for(SortedClusters::iterator cluster1_it = clusters.sortedClusters.begin(); cluster1_it != clusters.sortedClusters.end(); cluster1_it++){
		cluster_its[clusterId1] = cluster1_it;
		int clusterId2 = clusterId1+1;
		for(SortedClusters::iterator cluster2_it = next(cluster1_it); cluster2_it != clusters.sortedClusters.end(); cluster2_it++){
			unsigned int dist = convert_div_to_uintdiv(calcMaxDist(cluster1_it->second,cluster2_it->second));
			sortedDists[clusterId1].insert(make_pair(dist,clusterId2));
			sortedDists[clusterId2].insert(make_pair(dist,clusterId1));
			clusterId2++;
		}

		minDists.insert(make_pair(sortedDists[clusterId1].begin()->first,clusterId1));
		clusterId1++;
	}

	while(convert_uintdiv_to_div(minDists.begin()->first) < distThreshold && clusters.sortedClusters.size() > 1){

		multimap<unsigned int,int> newMinDists;

		int clusterId1 = minDists.begin()->second;
		int clusterId2 = sortedDists[clusterId1].begin()->second;

		unsigned int newuintDist = sortedDists[clusterId1].begin()->first;
		double newDist = convert_uintdiv_to_div(newuintDist);
		SortedClusters::iterator cluster1_it = cluster_its[clusterId1];
		SortedClusters::iterator cluster2_it = cluster_its[clusterId2];
		int cluster1Size = cluster1_it->second.size();
		int cluster2Size = cluster2_it->second.size();

		// Merge partitions
		vector<Partition *> mergedPartitions;
		mergedPartitions.reserve(cluster1Size+cluster2Size);
		mergedPartitions.insert(mergedPartitions.end(),make_move_iterator(cluster1_it->second.begin()),make_move_iterator(cluster1_it->second.end()));
		mergedPartitions.insert(mergedPartitions.end(),make_move_iterator(cluster2_it->second.begin()),make_move_iterator(cluster2_it->second.end()));

		clusters.maxClusterSize = max(clusters.maxClusterSize,static_cast<unsigned int>(cluster1Size+cluster2Size));
		clusters.sumMaxDist += newDist - cluster1_it->first - cluster2_it->first;

		// Delete and update old clusters and obsolete information
		clusters.sortedClusters.erase(cluster_its[clusterId1]);
		cluster_its[clusterId1] = clusters.sortedClusters.emplace(newDist,mergedPartitions);

		unordered_map<int,set<pair<unsigned int,int> > >::iterator cluster1SortedDists_it = sortedDists.find(clusterId1);
		for(set<pair<unsigned int,int> >::iterator it = next(cluster1SortedDists_it->second.begin()); it != cluster1SortedDists_it->second.end(); it++)
			sortedDists[it->second].erase(make_pair(it->first,clusterId1));
		cluster1SortedDists_it->second.clear();

		clusters.sortedClusters.erase(cluster_its[clusterId2]);
		cluster_its.erase(clusterId2);
		unordered_map<int,set<pair<unsigned int,int> > >::iterator cluster2SortedDists_it = sortedDists.find(clusterId2);
		for(set<pair<unsigned int,int> >::iterator it = cluster2SortedDists_it->second.begin(); it != cluster2SortedDists_it->second.end(); it++)
			if(it->second != clusterId1)
				sortedDists[it->second].erase(make_pair(it->first,clusterId2));
		sortedDists.erase(cluster2SortedDists_it);

		// Update priority queues
		for(multimap<unsigned int,int>::iterator it = next(minDists.begin()); it != minDists.end(); it++){
			int clusterId = it->second;
			if(clusterId != clusterId2){				
				unsigned int dist = convert_div_to_uintdiv(calcMaxDist(cluster_its[clusterId1]->second,cluster_its[clusterId]->second));
				sortedDists[clusterId1].insert(make_pair(dist,clusterId));
				sortedDists[clusterId].insert(make_pair(dist,clusterId1));
				newMinDists.insert(make_pair(sortedDists[clusterId].begin()->first,clusterId));
			}
		}


		newMinDists.insert(make_pair(sortedDists[clusterId1].begin()->first,clusterId1));
		swap(minDists,newMinDists);

	}

}


void Partitions::splitCluster(Clusters &clusters){
	// Modifies the order of cluster(s) such that the fist NsplitClu will be the centers.
	// Also, all elements will contain the partition it is closest to.

	vector<Partition *> &cluster = clusters.sortedClusters.begin()->second;
	unsigned int NpartitionsInCluster = cluster.size();

	// Find NsplitClu < NpartitionsInCluster new centers in updated medoids
	unsigned int Ncenters = 0;
	int firstCenterIndex = randInt(0,NpartitionsInCluster-1);
	
	// ************ Begin find partition proportional to distance from random partition
	Partition *clusterPartition = cluster[firstCenterIndex];
	vector<double> firstDist(NpartitionsInCluster);
	double distSum = 0.0;
	for(unsigned int i=0;i<NpartitionsInCluster;i++){
		double dist = wpJaccardDist(cluster[i],clusterPartition);
		firstDist[i] = dist;
		distSum += dist;
	}
	// Pick new center proportional to distance
	double randDist = randDouble(distSum);
	distSum = 0.0;
	for(unsigned int i=0;i<NpartitionsInCluster;i++){
		distSum += firstDist[i];
		if(distSum > randDist){
			firstCenterIndex = i;
			break;
		}
	}
	firstDist = vector<double>(0);

	// ************* End find partition proportional to distance from random partition
	cluster[firstCenterIndex]->minCenterPartition = cluster[firstCenterIndex];
	cluster[firstCenterIndex]->minDist = 0.0;

	// Put the center in first non-center position (Ncenters = 0) by swapping elements
	swap(cluster[Ncenters],cluster[firstCenterIndex]);
	Ncenters++;

	// Find NsplitClu-1 more centers based on the k++ algorithm
	while(Ncenters < NsplitClu){
		clusterPartition = cluster[Ncenters-1];
		distSum = 0.0;
		for(unsigned int i=Ncenters;i<NpartitionsInCluster;i++){
			double dist = wpJaccardDist(cluster[i],clusterPartition);
			if(dist < cluster[i]->minDist){
				// Found new min distance to center
				cluster[i]->minDist = dist;
				cluster[i]->minCenterPartition = clusterPartition;
			}
			distSum += cluster[i]->minDist;
		}
		// Pick new center proportional to distance
		randDist = randDouble(distSum);
		distSum = 0.0;
		unsigned int newCenterIndex = Ncenters;
		for(unsigned int i=Ncenters;i<NpartitionsInCluster;i++){
			distSum += cluster[i]->minDist;
			if(distSum > randDist){
				newCenterIndex = i;
				break;
			}
		}
		cluster[newCenterIndex]->minDist = 0.0;
		cluster[newCenterIndex]->minCenterPartition = cluster[newCenterIndex];
		// Put the center in first non-center position by swapping elements
		swap(cluster[Ncenters],cluster[newCenterIndex]);
		Ncenters++;
	}

	// Check if last center gives min distance for some partitions
	clusterPartition = cluster[Ncenters-1];
	for(unsigned int i=Ncenters;i<NpartitionsInCluster;i++){
		double dist = wpJaccardDist(cluster[i],clusterPartition);
		if(dist < cluster[i]->minDist){
			// Found new min distance to center
			cluster[i]->minDist = dist;
			cluster[i]->minCenterPartition = clusterPartition;
		}
		
	}
			
  // Identify new clusters
	unordered_map<int,pair<double,vector<Partition *> > > newClusters;
	unordered_map<int,pair<double,vector<Partition *> > >::iterator newClusters_it;

	for(unsigned int i=0;i<NpartitionsInCluster;i++){

		int centerId = cluster[i]->minCenterPartition->partitionId;
		// double minDist = cluster[i]->minDist;
		cluster[i]->minDist = bignum; // Reset for next iteration
		newClusters_it = newClusters.find(centerId);

		if(newClusters_it == newClusters.end()){
			pair<double,vector<Partition *> > newCluster;
			// newCluster.first = minDist;
			newCluster.second.push_back(cluster[i]);
			newClusters.emplace(make_pair(centerId,newCluster));
		}
		else{
			// newClusters_it->second.first += minDist;
			newClusters_it->second.second.push_back(cluster[i]);
		} 

	}

	// Remove the split cluster
	clusters.sumMaxDist -= clusters.sortedClusters.begin()->first;
	clusters.maxClusterSize = min(clusters.maxClusterSize,static_cast<unsigned int>(clusters.sortedClusters.begin()->second.size()));
	clusters.sortedClusters.erase(clusters.sortedClusters.begin());
	if(clusters.sortedClusters.empty())
		clusters.maxClusterSize = 0;
	
	// Add the new medoids
	for(newClusters_it = newClusters.begin(); newClusters_it != newClusters.end(); newClusters_it++){
		double maxDist = calcMaxDist(newClusters_it->second.second);
		newClusters_it->second.first = maxDist;
		clusters.sumMaxDist += newClusters_it->second.first;

		clusters.maxClusterSize = max(clusters.maxClusterSize,static_cast<unsigned int>(newClusters_it->second.second.size()));
		clusters.sortedClusters.emplace(newClusters_it->second);
	} 

	if(clusters.sortedClusters.begin()->first > splitDistThreshold)
		splitCluster(clusters);

}

void Partitions::readPartitionsFile(){

  cout << "Reading partitions file " << flush;  

  string line;  
  string buf;
  int lineNr = 0;

  // Count number of nodes and boot partitions
  while(getline(ifs,line)){
    lineNr++;
    if(lineNr > Nskiplines)
      break;
  }
  Nnodes++; // First line corresponds to first node

  istringstream read(line);
  while(read >> buf)
      Npartitions++;

  cout << "with " << Npartitions << " partitions " << flush;

  // Count remaining nodes
  while(getline(ifs,line))
    Nnodes++;
  if(ifs.bad())
    cout << "Error while reading file" << endl;

  cout << "of " << Nnodes << " nodes..." << flush;

  partitions = vector<Partition>(Npartitions);
  for(int i=0;i<Npartitions;i++)
  	partitions[i] = Partition(i,Nnodes);

  // Restart from beginning of file
  ifs.clear();
  ifs.seekg(0, ios::beg);

  // Read partitions data    
  int nodeNr = 0;
  lineNr = 0;
  while(getline(ifs,line)){
    lineNr++;
    if(lineNr > Nskiplines){
      istringstream read(line);
      int i = 0;
      while(read >> buf){
      	int assignment = atoi(buf.c_str());
        partitions[i].assignments[nodeNr] = assignment; 
        partitions[i].clusterSizes[assignment]++;
        i++;
      }
      nodeNr++;
    }
  }

  cout << "done!" << endl;

}

void Partitions::printClusters(){

	cout << "Writing clustering results..." << flush;

  my_ofstream ofs;
	ofs.open(outFileName.c_str());
	int i = 1;
	ofs << "# Clustered " << Npartitions << " partitions into " << bestClusters.sortedClusters.size() << " clusters with maximum distance " << bestClusters.sortedClusters.begin()->first << ", average maximum distance " << bestClusters.sumMaxDist/bestClusters.sortedClusters.size() << ", and maximum cluster size " << bestClusters.maxClusterSize << endl;
	ofs << "# ClusterId PartitionId" << endl;
	for(SortedClusters::iterator cluster_it = bestClusters.sortedClusters.begin(); cluster_it != bestClusters.sortedClusters.end(); cluster_it++){
		vector<Partition *> &cluster = cluster_it->second;
		int clusterSize = cluster.size();
		ofs << "# Cluster " << i << ": " << clusterSize << " nodes with max distance " << cluster_it->first << endl;

		set<int> orderedPartitionIds;
		for(vector<Partition *>::iterator partition_it = cluster.begin(); partition_it != cluster.end(); partition_it++)
			orderedPartitionIds.insert((*partition_it)->partitionId+1);

		for(set<int>::iterator id_it = orderedPartitionIds.begin(); id_it != orderedPartitionIds.end(); id_it++)
			ofs << i << " " << (*id_it) << endl;
		
		i++;
	}

	ofs.close();

	cout << "done!" << endl;

}

