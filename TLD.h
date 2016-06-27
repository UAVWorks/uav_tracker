#ifdef UAVtracker
#else
#define UAVtracker _declspec(dllimport)
#endif
#include <opencv2/opencv.hpp>
#include "tld_utils.h"
#include "LKTracker.h"
#include "FerNNClassifier.h"
#include <fstream>
#include"PatchGenerator.h"
#include<opencv2/tracking.hpp>
//#include <opencv2/legacy.hpp>
//#include<opencv2/features2d/features2d.hpp>
//#include<opencv2/opencv.hpp>
#include"kalman.h"
#include"Mp.h"
//Bounding Boxes
struct BoundingBox : public cv::Rect {
  BoundingBox(){}
  BoundingBox(cv::Rect r): cv::Rect(r){}
public:
  float overlap;        //Overlap with current Bounding Box
  int sidx;             //scale index
};

//Detection structure
struct DetStruct {
    std::vector<int> bb;
    std::vector<std::vector<int> > patt;
    std::vector<float> conf1;
    std::vector<float> conf2;
    std::vector<std::vector<int> > isin;
    std::vector<cv::Mat> patch;
  };
//Temporal structure��ʱ�ṹ
  struct TempStruct {
    std::vector<std::vector<int> > patt;
    std::vector<float> conf;
  };

struct OComparator{//�Ƚ����ߵ��غ϶�
  OComparator(const std::vector<BoundingBox>& _grid):grid(_grid){}
  std::vector<BoundingBox> grid;
  bool operator()(int idx1,int idx2){
    return grid[idx1].overlap > grid[idx2].overlap;
  }
};
struct CComparator{//�Ƚ��������Ŷ�
	CComparator(const std::vector<float>& _conf) :conf(_conf){}
	std::vector<float> conf;
	bool operator()(int idx1, int idx2){
		return conf[idx1]> conf[idx2];
	}
};

class UAVtracker TLD{
private:
  PatchGenerator generator;//������ͼ���������任
  FerNNClassifier classifier;
  LKTracker tracker;
  //������Щ����ͨ������ʼ����ʱ����parameters.yml�ļ����г�ʼ��  
  ///Parameters  
  int bbox_step;
  int min_win;
  int patch_size;

  //initial parameters for positive examples  
  //�ӵ�һ֡�õ���Ŀ���bounding box�У��ļ���ȡ�����û��򶨣����������α任��  
  //�� num_closest_init * num_warps_init 200��������  
  int num_closest_init;  //����ڴ����� 10  
  int num_warps_init;  //���α任��Ŀ 20  
  int noise_init;
  float angle_init;
  float shift_init;
  float scale_init;

  ////�Ӹ��ٵõ���Ŀ���bounding box�У��������α任��������������ӵ�����ģ�ͣ���  
  //update parameters for positive examples  
  int num_closest_update;
  int num_warps_update;
  int noise_update;
  float angle_update;
  float shift_update;
  float scale_update;

  //parameters for negative examples  
  float bad_overlap;
  float bad_patches;

  ///Variables  
  //Integral Images  ����ͼ�����Լ���2bitBP������������haar�����ļ��㣩  
  //Mat�������Ƹ�STL�����ƣ����Ƕ��ڴ���ж�̬�Ĺ�������Ҫ֮ǰ�û��ֶ��Ĺ����ڴ�  
  cv::Mat iisum;
  cv::Mat iisqsum;
  cv::Mat currentframe;/*********************************/
  float var;

  //Training data  
  //std::pair��Ҫ�������ǽ�����������ϳ�һ�����ݣ��������ݿ�����ͬһ���ͻ��߲�ͬ���͡�  
  //pairʵ������һ���ṹ�壬����Ҫ��������Ա������first��second����������������ֱ��ʹ�á�  
  //������������ʾ������first��ԱΪ features ���������飬second��ԱΪ labels ��������ǩ  
  std::vector<std::pair<std::vector<int>, int> > pX; //positive ferns <features,labels=1> ���Ϸ����� ������  
  std::vector<std::pair<std::vector<int>, int> > nX; // negative ferns <features,labels=0> ���Ϸ����� ������  
  cv::Mat pEx;  //positive NN example    ����ڷ����������� ֻ��һ��
  std::vector<cv::Mat> nEx; //negative NN examples  ����ڷ����������� ���

  //Test data   
  std::vector<std::pair<std::vector<int>, int> > nXT; //negative data to Test  
  std::vector<cv::Mat> nExT; //negative NN examples to Test  

  //Last frame data  
  BoundingBox lastbox;
  bool lastvalid;
  float lastconf;

  //Current frame data  
  //Tracker data  
  bool tracked;
  BoundingBox tbb;
  Rect2d kcfbox;
  bool tvalid;
  float tconf;
  Ptr<cv::Tracker> kcf;
  //Detector data 
  TempStruct tmp;
  DetStruct dt;
  std::vector<BoundingBox> dbb;
  std::vector<bool> dvalid;   //�����Ч�ԣ���  
  std::vector<float> dconf;  //���ȷ�Ŷȣ���  
  bool detected;

  //Bounding Boxes
  std::vector<BoundingBox> grid;
  std::vector<BoundingBox> gridoftracker;/***************************/
  int numl = 0;
  int numr = 0;
  int numj = 0;
  std::vector<int> gridscalnum;
  std::vector<cv::Size> scales;
  std::vector<int> good_boxes; //indexes of bboxes with overlap > 0.6
  std::vector<int> bad_boxes; //indexes of bboxes with overlap < 0.2
  BoundingBox bbhull; // hull of good_boxes ������ı߿�
  BoundingBox best_box; // maximum overlapping bbox
  //kalman�˲���
  kalman Kalman;
  //Mp�˲���
	 Mp Mp1;//ˮƽ����
  Mp Mp2;//��ֱ����
  std::vector<int> kalman_boxes;//kalman�˲�ȷ���Ĵ�������ͼ���ı��
public:
  //Constructors
  TLD();
  TLD(const cv::FileNode& file);
  void read(const cv::FileNode& file);
  //Methods
  void init(cv::Mat& frame1,const cv::Rect &box, FILE* bb_file);
  void generatePositiveData(const cv::Mat& frame, int num_warps);
  void generateNegativeData(const cv::Mat& frame);
  void processFrame(const cv::Mat& frame,const cv::Mat& img1,const cv::Mat& img2,std::vector<cv::Point2f>& points1,std::vector<cv::Point2f>& points2,
      BoundingBox& bbnext,bool& lastboxfound, bool tl,bool tk,bool tm,FILE* bb_file);
  void track(const cv::Mat& img1, const cv::Mat& img2,std::vector<cv::Point2f>& points1,std::vector<cv::Point2f>& points2, cv::Point2f& pt1,cv::Point2f& pt2);
  void detect(const cv::Mat& frame,bool lastboxfound,bool tk,bool tm,float spdirection ,float czdirection);
  void detect1(const cv::Mat& frame,Point2f& predictpt,float spdirection,float czdirection,bool lastboxfound, bool tk, bool tm);
  void detect2(const cv::Mat& frame, Point2f& predictpt, float spdirection, float czdirection, bool lastboxfound, bool tk, bool tm);
  void detect3(const cv::Mat& frame,int detections);
  void detect4(const cv::Mat& frame,int detections);
  void clusterConf(const std::vector<BoundingBox>& dbb,const std::vector<float>& dconf,std::vector<BoundingBox>& cbb,std::vector<float>& cconf);
  void evaluate();
  void learn(const cv::Mat& img,bool tk);
  //Tools
  void buildGrid(const cv::Mat& img, const cv::Rect& box);
  void changeGrid(BoundingBox& tobb);
  float bbOverlap(const BoundingBox& box1,const BoundingBox& box2);
  void getOverlappingBoxes(const cv::Rect& box1,int num_closest);
  void getOverlappingBoxes(const cv::Rect& box1, int num_closest,bool tk);
  void getBBHull();
  void getPattern(const cv::Mat& img, cv::Mat& pattern,cv::Scalar& mean,cv::Scalar& stdev);
  void bbPoints(std::vector<cv::Point2f>& points, const BoundingBox& bb);
  void bbPredict(const std::vector<cv::Point2f>& points1,const std::vector<cv::Point2f>& points2,
  const BoundingBox& bb1,BoundingBox& bb2);
  double getVar(const BoundingBox& box,const cv::Mat& sum,const cv::Mat& sqsum);
  bool bbComp(const BoundingBox& bb1,const BoundingBox& bb2);
  int clusterBB(const std::vector<BoundingBox>& dbb,std::vector<int>& indexes);
  void trainTh(Mat&img);
  
};
int max(int a,int b);
