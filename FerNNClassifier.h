/*
 * FerNNClassifier.h
 *
 *  Created on: Jun 14, 2011
 *      Author: alantrrs
 */
#ifdef UAVtracker
#else
#define UAVtracker _declspec(dllimport)
#endif
#include <opencv2/opencv.hpp>
#include <stdio.h>

class UAVtracker FerNNClassifier{
private:
	//������Щ����ͨ������ʼ����ʱ����parameters.yml�ļ����г�ʼ��  
  float thr_fern;//0.6
  int structSize;
  int nstructs;
  float valid;
  float ncc_thesame;
  float thr_nn;//0.65
  int acum;
public:
  //Parameters
	cv::Mat pmodel;//���Ŀ��ģ��pEx��mat
  float thr_nn_valid;//��ǰ����ڷ�������ֵ����Чֵ����=ǰһ֡��ֵ 0.7
  bool model_flag=true;//�Ƿ���ӵ�Ŀ��ģ�͵ı�־λ
  void read(const cv::FileNode& file);
  void prepare(const std::vector<cv::Size>& scales);
  void getFeatures(const cv::Mat& image,const int& scale_idx,std::vector<int>& fern);
  void update(const std::vector<int>& fern, int C, int N);
  float measure_forest(std::vector<int> fern);
  void trainF(const std::vector<std::pair<std::vector<int>,int> >& ferns,int resample);
  void trainNN(const std::vector<cv::Mat>& nn_examples, const std::vector<cv::Mat>& nn_images);
  void NNConf(const cv::Mat& example,std::vector<int>& isin,float& rsconf,float& csconf,float& msconf);
//  void NNConf1(const cv::Mat& example, std::vector<int>& isin, float& rsconf, float& csconf);
  void evaluateTh(const std::vector<std::pair<std::vector<int>,int> >& nXT,const std::vector<cv::Mat>& nExT);
  void show();
  //Ferns Members
  int getNumStructs(){return nstructs;}
  float getFernTh(){return thr_fern;}
  float getNNTh(){return thr_nn;}
  struct Feature
      {
          uchar x1, y1, x2, y2;
          Feature() : x1(0), y1(0), x2(0), y2(0) {}	//ð�ź���Ĵ����ʼ��
          Feature(int _x1, int _y1, int _x2, int _y2)
          : x1((uchar)_x1), y1((uchar)_y1), x2((uchar)_x2), y2((uchar)_y2)
          {}
          bool operator ()(const cv::Mat& patch) const
			  //��ά��ͨ��Ԫ�ؿ�����Mat::at(i, j)���ʣ�i������ţ�j�������  
			  //���ص�patchͼ��Ƭ��(y1,x1)��(y2, x2)������رȽ�ֵ������0����1
          { return patch.at<uchar>(y1,x1) > patch.at<uchar>(y2, x2); }
      };
  //Ferns��ާ��ֲ��и�������Ҷ֮�֣����߻���features �����飿 
  std::vector<std::vector<Feature> > features; //Ferns features (one std::vector for each scale)
  std::vector< std::vector<int> > nCounter; //negative counter
  std::vector< std::vector<int> > pCounter; //positive counter
  std::vector< std::vector<float> > posteriors; //Ferns posteriors//��άvector
  float thrN; //Negative threshold
  float thrP;  //Positive thershold
  //NN Members
  std::vector<cv::Mat> pEx; //NN positive examples
  std::vector<cv::Mat> nEx; //NN negative examples
};
