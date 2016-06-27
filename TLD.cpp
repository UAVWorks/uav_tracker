/*
 * TLD.cpp
 *
 *  Created on: Jun 9, 2011
 *      Author: alantrrs
 */
#define UAVtracker _declspec(dllexport)
#include "TLD.h"
#include <stdio.h>
#include<time.h>
using namespace cv;
using namespace std;
//float SCALES[] = { 0.57870, 0.69444, 0.83333, 1, 1.20000, 1.44000, 1.72800 };
float SCALES[] = { 0.16151, 0.19381, 0.23257, 0.27908, 0.33490, 0.40188, 0.48225,
0.57870, 0.69444, 0.83333, 1, 1.20000, 1.44000, 1.72800,
2.07360, 2.48832, 2.98598, 3.58318, 4.29982, 5.15978, 6.19174 }; 
//float SCALES[] = { 0.33490, 0.40188, 0.48225,
//0.57870, 0.69444, 0.83333, 1, 1.20000, 1.44000, 1.72800,
//2.07360, 2.48832, 2.98598, 3.58318, 4.29982};
TLD::TLD()
{
}
TLD::TLD(const FileNode& file){
  read(file);
}

void TLD::read(const FileNode& file){
  ///Bounding Box Parameters
  min_win = (int)file["min_win"];//15
  ///Genarator Parameters
  //initial parameters for positive examples
  patch_size = (int)file["patch_size"];//15
  num_closest_init = (int)file["num_closest_init"];//10
  num_warps_init = (int)file["num_warps_init"];//20
  noise_init = (int)file["noise_init"];//5
  angle_init = (float)file["angle_init"];//20
  shift_init = (float)file["shift_init"];//0.02
  scale_init = (float)file["scale_init"];//0.02
  //update parameters for positive examples
  num_closest_update = (int)file["num_closest_update"];//10
  num_warps_update = (int)file["num_warps_update"];//10
  noise_update = (int)file["noise_update"];//5
  angle_update = (float)file["angle_update"];//10
  shift_update = (float)file["shift_update"];//0.02
  scale_update = (float)file["scale_update"];//0.02
  //parameters for negative examples
  bad_overlap = (float)file["overlap"];//0.2
  bad_patches = (int)file["num_patches"];//100 x:288 y:36 w:25 h:42
  classifier.read(file);
}

void TLD::init( Mat& frame1,const Rect& box,FILE* bb_file){
	kcf = Tracker::create("KCF");
	kcfbox = Rect2d(box.x, box.y, box.width, box.height);
	kcf->init(frame1, kcfbox);
	cvtColor(frame1, frame1, CV_RGB2GRAY);
  //bb_file = fopen("bounding_boxes.txt","w");
  //Get Bounding Boxes
	//�˺������ݴ����box��Ŀ��߽���ڴ����ͼ��frame1�й���ȫ����ɨ�贰�ڣ��������ص��ȣ��ص��ȶ���Ϊ����box�Ľ����벢��֮��
	clock_t t1 = clock();
    buildGrid(frame1,box);
	clock_t t2 = clock();
	//cout << "����grid��ʱ" << t2 - t1 << "ms" << endl;
   // printf("Created %d bounding boxes\n",(int)grid.size());//vector�ĳ�Աsize()���ڻ�ȡ����Ԫ�صĸ���  
	/*for (int i = 0; i < gridscalnum.size(); i++)
		cout << "��" << i + 1 << "�ֳ߶ȸ���=" << gridscalnum[i] << endl;*/
	frame1.copyTo(currentframe);
  ///Preparation
  //allocation
 //����ͼ�����Լ���2bitBP������������haar�����ļ��㣩  
 //Mat�Ĵ�������ʽ�����֣�1.����create���У��У����ͣ�2.Mat���У��У����ͣ�ֵ������
  iisum.create(frame1.rows+1,frame1.cols+1,CV_32F);//float����
  iisqsum.create(frame1.rows+1,frame1.cols+1,CV_64F);//double����
  //Detector data�ж��壺std::vector<float> dconf; 
  //vector ��reserve������vector��capacity����������sizeû�иı䣡��resize�ı���vector  
  //��capacityͬʱҲ����������size��reserve������Ԥ���ռ䣬���ڿռ��ڲ���������Ԫ�ض���  
  //������û������µĶ���֮ǰ���������������ڵ�Ԫ�ء�  
  //�����ǵ���resize����reserve�����߶�����ԭ�е�Ԫ�ض�û��Ӱ�졣  
  //myVec.reserve( 100 );     // ��Ԫ�ػ�û�й���, ��ʱ������[]����Ԫ��  
  //myVec.resize( 100 );      // ��Ԫ�ص�Ĭ�Ϲ��캯��������100���µ�Ԫ�أ�����ֱ�Ӳ�����Ԫ��  
  dconf.reserve(100);  //ȷ�Ŷ�
  dbb.reserve(100);	//��Ч��
  bbox_step =7;

  // ������Detector data�ж�����������������grid.size()��С�������һ��ͼ����ȫ����ɨ�贰�ڸ�����������
  //Detector data�ж���TempStruct tmp;    
  //tmp.conf.reserve(grid.size()); 
  tmp.conf = vector<float>(grid.size());	//
  tmp.patt = vector<vector<int> >(grid.size(),vector<int>(10,0));
  //tmp.patt.reserve(grid.size());
  dt.bb.reserve(grid.size());
  good_boxes.reserve(grid.size());
  bad_boxes.reserve(grid.size());
  kalman_boxes.reserve(grid.size());

  //TLD�ж��壺cv::Mat pEx;  //positive NN example ��СΪ15*15ͼ��Ƭ  
  pEx.create(patch_size,patch_size,CV_64F);

  //Init Generator
  //TLD�ж��壺cv::PatchGenerator generator;  //PatchGenerator��������ͼ��������з���任  
  /*
  cv::PatchGenerator::PatchGenerator (
  double     _backgroundMin,
  double     _backgroundMax,
  double     _noiseRange,
  bool     _randomBlur = true,
  double     _lambdaMin = 0.6,
  double     _lambdaMax = 1.5,
  double     _thetaMin = -CV_PI,
  double     _thetaMax = CV_PI,
  double     _phiMin = -CV_PI,
  double     _phiMax = CV_PI
  )
  һ����÷����ȳ�ʼ��һ��PatchGenerator��ʵ����Ȼ��RNGһ��������ӣ��ٵ��ã������������һ���任�����������
  */
  //]
  generator = PatchGenerator (0,0,noise_init,true,1-scale_init,1+scale_init,-angle_init*CV_PI/180,angle_init*CV_PI/180,-angle_init*CV_PI/180,angle_init*CV_PI/180);

  //�˺������ݴ����box��Ŀ��߽�򣩣�����֡ͼ���е�ȫ��������Ѱ�����box������С���������ƣ�  
  //�ص�����󣩵�num_closest_init�����ڣ�Ȼ�����Щ���� ����good_boxes����  
  //ͬʱ�����ص���С��0.2�ģ����� bad_boxes ����  
  //���ȸ���overlap�ı�����Ϣѡ���ظ������������60%����ǰnum_closet_init= 10������ӽ�box��RectBox��  
  //�൱�ڶ�RectBox����ɸѡ����ͨ��BBhull�����õ���ЩRectBox�����߽硣  
   t1 = clock();
  getOverlappingBoxes(box, num_closest_init);
   t2 = clock();
  //cout << "Overlapping       time            is     " << t2 - t1 << "ms!!!!!" << endl;
  //��ʼ��ʱ��Զ����10��good_boxes//�����������60000���������

  // printf("Found %d good boxes, %d bad boxes\n",(int)good_boxes.size(),(int)bad_boxes.size());
 // printf("Best Box: %d %d %d %d\n",best_box.x,best_box.y,best_box.width,best_box.height);
  //printf("Bounding box hull: %d %d %d %d\n",bbhull.x,bbhull.y,bbhull.width,bbhull.height);
  //Correct Bounding Box
  lastbox=best_box;
  lastconf=1;
  lastvalid=true;
  //Print
 // fprintf(bb_file,"%d,%d,%d,%d,%f\n",lastbox.x,lastbox.y,lastbox.br().x,lastbox.br().y,lastconf);
  //Prepare Classifier
  //scales������������ɨ�贰�ڵĳ߶ȣ���buildGrid()������ʼ��  
  classifier.prepare(scales);

  ///Generate Data
  // Generate positive data
  //��good_boxes������10*20������������
  generatePositiveData(frame1,num_warps_init);

  // Set variance threshold
  Scalar stdev, mean;//��׼���ֵ
  //ͳ��best_box�ľ�ֵ�ͱ�׼��  
  ////������Ҫ��ȡͼ��A��ĳ��ROI������Ȥ�����ɾ��ο򣩵Ļ�����Mat���B=img(ROI)������ȡ  
  //frame1(best_box)�ͱ�ʾ��frame1����ȡbest_box����Ŀ�����򣩵�ͼ��Ƭ  
  meanStdDev(frame1(best_box),mean,stdev);

  //���û���ͼ��ȥ����ÿ������ⴰ�ڵķ���  
  //cvIntegral( const CvArr* image, CvArr* sum, CvArr* sqsum=NULL, CvArr* tilted_sum=NULL );  
  //�������ͼ������ͼ��sum����ͼ��, W+1��H+1��sqsum������ֵƽ���Ļ���ͼ��tilted_sum��ת45�ȵĻ���ͼ��  
  //���û���ͼ�񣬿��Լ�����ĳ���ص��ϣ��ҷ��Ļ�����ת�ľ��������н�����͡����ֵ�Լ���׼����ļ��㣬  
  //���ұ�֤����ĸ��Ӷ�ΪO(1)��  
  integral(frame1,iisum,iisqsum);

  //����������ģ��һ��������ģ��
  //���û���ͼ����ÿ������ⴰ�ڵķ������С��var��ֵ��Ŀ��patch�����50%���ģ�  
  //����Ϊ�京��ǰ��Ŀ�귽�var Ϊ��׼���ƽ��  
  var = pow(stdev.val[0],2)*0.5; //getVar(best_box,iisum,iisqsum);best_box�ı�׼�� *0.5
 // cout << "variance: " << var << endl;
  //check variance
  //getVar����ͨ������ͼ����������best_box�ķ���  
  double vr =  getVar(best_box,iisum,iisqsum)*0.5;//�̶��㷨���ճ���
  //cout << "check variance: " << vr << endl;
  //����������Ӧ�����


  // Generate negative data
  generateNegativeData(frame1);
  //Split Negative Ferns into Training and Testing sets (they are already shuffled)
  int half = (int)nX.size()*0.5f;//f��������
  //vector::assign����������[start, end)�е�ֵ��ֵ����ǰ��vector.  
  //��һ��ĸ������� ��Ϊ ���Լ�  
  nXT.assign(nX.begin()+half,nX.end());//nXT�����Ϸ������ĸ��������Լ�
  nX.resize(half);                     //nX: ���Ϸ������ĸ�������ѵ������
  ///Split Negative NN Examples into Training and Testing sets
  //���������Ž�ѵ�����Ͳ��Լ�  
  half = (int)nEx.size()*0.5f;
  nExT.assign(nEx.begin()+half,nEx.end());//nExT:����ڷ������ĸ��������Լ�
  nEx.resize(half);                       //nExT:����ڷ������ĸ�������ѵ������

  //Merge Negative Data with Positive Data and shuffle it
  //�ϲ���������������
  vector<pair<vector<int>,int> > ferns_data(nX.size()+pX.size());
  vector<int> idx = index_shuffle(0,ferns_data.size());
  int a=0;
  for (int i=0;i<pX.size();i++){
      ferns_data[idx[a]] = pX[i];
      a++;
  }
  for (int i=0;i<nX.size();i++){
      ferns_data[idx[a]] = nX[i];
      a++;
  }

  //Data already have been shuffled, just putting it in the same vector
  vector<cv::Mat> nn_data(nEx.size()+1);//pEx=1
  vector<cv::Mat> nn_image(nEx.size() + 1);//pEx=1
  nn_data[0] = pEx;
  nn_image[0] = frame1(lastbox);
  for (int i=0;i<nEx.size();i++){
      nn_data[i+1]= nEx[i];
		 int idx = bad_boxes[i];
	nn_image[i+1] = frame1(grid[idx]);
  }
  ///Training
  t1 = clock();
  classifier.trainF(ferns_data,2); //bootstrap = 2
  t2 = clock();
  //cout << "ѵ��Fern����������" << t2 - t1 << "ms" << endl;
  classifier.trainNN(nn_data,nn_image);
  ///Threshold Evaluation on testing sets
  classifier.evaluateTh(nXT,nExT);
  //kalman�˲����ĳ�ʼ��
  Point2f initpoint;
  initpoint.x = (float)lastbox.x + (float)lastbox.width / 2.f;
  initpoint.y = (float)lastbox.y + (float)lastbox.height / 2.f;
  //cout << initpoint.x <<endl<< initpoint.y << endl;
  Kalman.kalmaninit(initpoint);
  Mp1.init(initpoint, 0);
  Mp2.init(initpoint, 1);

}

/* Generate Positive data
 * Inputs:
 * - good_boxes (bbP)
 * - best_box (bbP0)
 * - frame (im0)
 * Outputs:
 * - Positive fern features (pX)
 * - Positive NN examples (pEx)
 */
void TLD::generatePositiveData(const Mat& frame, int num_warps){
	/*
	CvScalar����ɴ��1��4����ֵ����ֵ���������洢���أ���ṹ�����£�
	typedef struct CvScalar
	{
	double val[4];
	}CvScalar;
	���ʹ�õ�ͼ����1ͨ���ģ���s.val[0]�д洢����
	���ʹ�õ�ͼ����3ͨ���ģ���s.val[0]��s.val[1]��s.val[2]�д洢����
	*/
  Scalar mean;
  Scalar stdev;
  //�˺�����frameͼ��best_box�����ͼ��Ƭ��һ��Ϊ��ֵΪ0��15*15��С��patch������pEx�������У�pEX��Ϊ�˺��������ڷ�������׼����  
  getPattern(frame(lastbox),pEx,mean,stdev);
  //Get Fern features on warped patches
  Mat img;
  Mat warped;//���� 
  GaussianBlur(frame,img,Size(9,9),1.5);

  //��imgͼ���н�ȡbbhull��Ϣ��bbhull�ǰ�����λ�úʹ�С�ľ��ο򣩵�ͼ�񸳸�warped  
  //������Ҫ��ȡͼ��A��ĳ��ROI������Ȥ�����ɾ��ο򣩵Ļ�����Mat���B=img(ROI)������ȡ  
  warped = img(bbhull);//warped��img bbhull�����ǳ�����������ǹ���洢�ռ�ģ�����warpedһ�䣬imgҲ�ͻ�䡣
  RNG& rng = theRNG();
  Point2f pt(bbhull.x + (bbhull.width - 1)*0.5f, bbhull.y + (bbhull.height - 1)*0.5f); //ȡ���ΰ�Χ�����ĵ�����  int i(2) 
  
  //nstructs��ľ����һ�������鹹����ÿ����������ͼ���Ĳ�ͬ��ͼ��ʾ���ĸ���  
  //fern[nstructs] nstructs������ɭ�ֵ����飿��  
  vector<int> fern(classifier.getNumStructs());
  pX.clear();
  Mat patch;
  //pXΪ������RectBox���߽紦����������Ϣ��pEx����ڵ�RectBox��Pattern��bbP0Ϊ����ڵ�RectBox��  
  if (pX.capacity()<num_warps*good_boxes.size())//20*10
    pX.reserve(num_warps*good_boxes.size());//pX����������Ϊ ����任���� * good_box�ĸ������������������ô��Ŀռ�  200
  int idx;
  clock_t t1 = clock();
  for (int i=0;i<num_warps;i++)//20�ּ��α任 //���Ѹĳ�10��
  {
	  
	  if (i>0)
	  {
		  generator(frame, pt, warped, bbhull.size(), rng); //PatchGenerator��������ͼ��������з���任����RNGһ��������ӣ��ٵ��ã������������һ���任�����������   //��frame��ѡ��������з���任��������任������浽warped��
		/* imshow("1", warped);
		  waitKey(0);*/
	  }
		
	  for (int b = 0; b < good_boxes.size(); b++)
	  {
		  idx = good_boxes[b];//good_boxes����������� grid ������ 
		  //Rect region(grid[idx].x-bbhull.x,grid[idx].y-bbhull.y,grid[idx].width,grid[idx].height);
		  patch = img(grid[idx]);//��img�� grid[idx] ����Ҳ����bounding box�ص��ȸߵģ���һ��ͼ��Ƭ��ȡ����  
		  //getFeatures�����õ������patch���������Ľڵ㣬Ҳ���������������fern��13λ�Ķ����ƴ��룩  
		  classifier.getFeatures(patch, grid[idx].sidx, fern);//��10��good_boxes��ͼ������ �õ���Ӧ�߶�box���ǵ�ͼ��� �����������������ֵ��fern��һ����10��fern����ÿ�������13������ֵ��һ��ʮ������ֵ����13λ�Ķ�����������ʾ��
		  pX.push_back(make_pair(fern, 1));//�����͵õ�һ��good_box��������10��*13��������Ե�����ֵ��������������Ҫ��200����
		  /*cout << "��" << b << "box" << "��";
		  for (int j = 0; j < pX[b].first.size();j++)
		  cout <<"��"<<j<<"������"<<"����ֵ"<<hex<< pX[b].first[j]<<" ";
		  cout << endl;*/
	  }
	 /* cout << endl;*/
	   //generator(warped, Point2f(warped.rows*0.5f, warped.cols*0.5f), warped, bbhull.size(), rng);
  }
  clock_t t2 = clock();
 // cout << "10�ּ��α任ʱ��Ϊ" << t2 - t1 << "ms" << endl;

  //printf("%d��good_boxes���������ɭ��������%d��\n", good_boxes.size(),(int)pX.size());
}

//�ȶ���ӽ�box��RectBox����õ���patch ,Ȼ��������Ϣת��ΪPattern��  
//�����˵���ǹ�һ��RectBox��Ӧ��patch��size��������patch_size = 15*15������2ά�ľ�����һά��������Ϣ��  
//Ȼ��������Ϣ��ֵ��Ϊ0������Ϊzero mean and unit variance��ZMUV��  
//Output: resized Zero-Mean patch  
void TLD::getPattern(const Mat& img, Mat& pattern,Scalar& mean,Scalar& stdev){
  //Output: resized Zero-Mean patch
  resize(img,pattern,Size(patch_size,patch_size));//��img������patch_size = 15*15���浽pattern��  
  //�������15*15����ľ�ֵ�ͱ�׼��
  meanStdDev(pattern,mean,stdev);
  pattern.convertTo(pattern,CV_32F);
  pattern = pattern-mean.val[0];//opencv��Mat������������أ� Mat���� + Mat; + Scalar; + int / float / double ������  
  //����������Ԫ�ؼ�ȥ���ֵ��Ҳ���ǰ�patch�ľ�ֵ��Ϊ��  
 
}

void TLD::generateNegativeData(const Mat& frame){
/* Inputs:
 * - Image
 * - bad_boxes (Boxes far from the bounding box)
 * - variance (pEx variance)
 * Outputs
 * - Negative fern features (nX)
 * - Negative NN examples (nEx)
 */
	//����֮ǰ�ص���С��0.2�ģ������� bad_boxes�ˣ���������ͦ�࣬����ĺ������ڴ���˳��Ҳ����Ϊ��  
	//�������ѡ��bad_boxes  
  random_shuffle(bad_boxes.begin(),bad_boxes.end());//Random shuffle bad_boxes indexes
  int idx;
  //Get Fern Features of the boxes with big variance (calculated using integral images)
  int a=0;//��������
  //int num = std::min((int)bad_boxes.size(),(int)bad_patches*100); //limits the size of bad_boxes to try
  //printf("negative data generation started.\n");
  vector<int> fern(classifier.getNumStructs());
  nX.reserve(bad_boxes.size());
  Mat patch;
  for (int j=0;j<bad_boxes.size()*0.1;j++)
  {
      idx = bad_boxes[j];
	  if (getVar(grid[idx], iisum, iisqsum)<var*0.5f) //�ѷ���ϴ��bad_boxes���븺���� 
            continue;
      patch =  frame(grid[idx]);
	  classifier.getFeatures(patch,grid[idx].sidx,fern);
      nX.push_back(make_pair(fern,0));//�õ�������  
      a++;
  }
  //printf("%d��bad_boxesǰʮ��֮һ���������ɭ�֣�ާ��������%d��\n ", bad_boxes.size(),a);
  //random_shuffle(bad_boxes.begin(),bad_boxes.begin()+bad_patches);//Randomly selects 'bad_patches' and get the patterns for NN;
  Scalar dum1, dum2;
  nEx=vector<Mat>(bad_patches);//100
  for (int i=0;i<bad_patches;i++)
  {
      idx=bad_boxes[i];
	  patch = frame(grid[idx]);
	  //�����˵���ǹ�һ��RectBox��Ӧ��patch��size��������patch_size = 15*15��  
	  //���ڸ���������Ҫ��ֵ�ͷ�����ԾͶ���dum����������  
      getPattern(patch,nEx[i],dum1,dum2);
  }
  //printf("����ڷ�����������: %d��\n",(int)nEx.size());
}

//�ú���ͨ������ͼ����������box�ķ��� 
double TLD::getVar(const BoundingBox& box,const Mat& sum,const Mat& sqsum)
{
  double brs = sum.at<int>(box.y+box.height,box.x+box.width);
  double bls = sum.at<int>(box.y+box.height,box.x);
  double trs = sum.at<int>(box.y,box.x+box.width);
  double tls = sum.at<int>(box.y,box.x);
  double brsq = sqsum.at<double>(box.y+box.height,box.x+box.width);
  double blsq = sqsum.at<double>(box.y+box.height,box.x);
  double trsq = sqsum.at<double>(box.y,box.x+box.width);
  double tlsq = sqsum.at<double>(box.y,box.x);
  double mean = (brs+tls-trs-bls)/((double)box.area());
  double sqmean = (brsq+tlsq-trsq-blsq)/((double)box.area());
  return sqmean-mean*mean;
}

void TLD::processFrame(const cv::Mat& frame, const cv::Mat& img1, const cv::Mat& img2, vector<Point2f>& points1, vector<Point2f>& points2, BoundingBox& bbnext, bool& lastboxfound, bool tl, bool tk, bool tm, FILE* bb_file)
{
	vector<BoundingBox>cbb;//����֮���bounding box  
	vector<float>cconf;
	int confident_detections = 0;//СD�Ľ������֮�󣬷�����СT�ߵ���Ŀ  
	int didx; //detection index  
	/// 1.Track  
	Point2f trackpt1 = { 0 };//ǰ������ø��ٵ㣬���Ը�������ɷ�ģ��
	Point2f trackpt2 = { 0 };
	if (lastboxfound&&tl)//ǰһ֡Ŀ����ֹ������ǲŸ��٣�����ֻ�ܼ����  
	{
		clock_t t1 = clock();
		
		//Rect2d tbb1 = Rect2d(tbb.x, tbb.y, tbb.width, tbb.height);
		if (tm)
		{
			tracked = kcf->update(frame, kcfbox);
			if (tracked)
			{
				//kcfbox = kcfbox&Rect2d(0, 0, img2.rows, img2.cols);
				tbb.x = kcfbox.x;
				tbb.y = kcfbox.y;
				tbb.width = kcfbox.width;
				tbb.height = kcfbox.height;

				Mat pattern;
				Scalar mean, stdev;
				//tbb = tbb&Rect(0, 0, img2.rows, img2.cols);
				tbb.x = max(tbb.x, 0);
				tbb.y = max(tbb.y, 0);
				tbb.width = min(min(img2.cols - tbb.x, tbb.width), min(tbb.width, tbb.br().x));
				tbb.height = min(min(img2.rows - tbb.y, tbb.height), min(tbb.height, tbb.br().y));
				if (tbb.width < 5 || tbb.height < 5)
				{
					tracked = false;

				}
				else
				{
					//Mat track = img2(tbb);
					getPattern(img2(tbb), pattern, mean, stdev);
					vector<int> isin;
					float dummy, conf;
					//����ͼ��Ƭpattern������ģ��M�ı������ƶ�  
					classifier.NNConf(pattern, isin, conf, dummy, tconf); //Conservative Similarity tconf����csconf ������ڷ�������Conservative Similarity��5.2����Ϊ����Ŀ��ĵ÷ּ��ɣ�����Ҫ������÷ֺͼ�������бȽϡ�
					tvalid = lastvalid;
					//�������ƶȴ�����ֵ��������������Ч 
					cout << tconf << endl;
					if (tconf > 0.5)
						tvalid = true;//�ж��켣�Ƿ���Ч���Ӷ������Ƿ�Ҫ��������������־λtvalid������5.6.2 P-Expert�� 
					if (tconf < 0.5)
						tvalid = false;
					/*if (tconf < 0.35)
						tracked = false;*/
					clock_t t2 = clock();
				}
				//cout << "track����" << t2 - t1 << "ms" << endl;
			}
			//cout << tvalid << endl;
		}
		else
		{
			track(img1, img2, points1, points2, trackpt1, trackpt2);//����������㣨���Ȳ���������lastbox�й��������10*10=100�������㣬����points1
		}
	}
	
	else //lastfound=false
	{
		tracked = false;
			//tvalid = false;
	}
	
		///Detect
		/*if (tracked){
			printf("%f\t%f\n", trackpt1.x, trackpt1.y);
			printf("%f\t%f\n", trackpt2.x, trackpt2.y);
			}*/
		float spdirection = 1.f;
		float czdirection = 1.f;
		//cout << tm<<endl;
		//cout << tracked<<endl;
		/*if (tm&&tracked)
		{
			if (trackpt1.x < trackpt2.x)
				spdirection = 1.f;
			else if (trackpt1.x > trackpt2.x)
				spdirection = 0.f;
			else
				spdirection = 0.5;
			if (trackpt1.y < trackpt2.y)
				czdirection = 1.f;
			else if (trackpt1.y > trackpt2.y)
				czdirection = 0.f;
			else
				czdirection = 0.5f;
		}
		if (tm&&!tracked)
		{
			spdirection = Mp1.output();
			czdirection = Mp2.output();
		}*/
		clock_t t1 = clock(); 
			detect(img2, lastboxfound, tk, tm, spdirection, czdirection);
			if (tconf > 0.55&&tracked == true)
				detected = false;

	/*	if (tconf < 0.5 || tracked ==false)
		detect(img2, lastboxfound, tk, tm, spdirection, czdirection);
		else
		{
			detected = false;
		}
		*/
		clock_t t2 = clock();
	
		//cout << "detect����" << t2 - t1 << "ms" << endl;
		///Integration 
		///Integration   �ۺ�ģ��  
		//TLDֻ���ٵ�Ŀ�꣬�����ۺ�ģ���ۺϸ��������ٵ��ĵ���Ŀ��ͼ������⵽�Ķ��Ŀ�꣬Ȼ��ֻ����������ƶ�����һ��Ŀ��  
		t1 = clock();
		if (tracked)
		{
			bbnext = tbb;
			lastconf = tconf;//��ʾ������ƶȵ���ֵ
			lastvalid = tvalid;
			// printf("Tracked\n");
			if (detected)
			{ //ͨ�� �ص��� �Լ������⵽��Ŀ��bounding box���о��࣬ÿ�������ص���С��0.5 //   if Detected
				clusterConf(dbb, dconf, cbb, cconf);                       //   cluster detections
				//  printf("�ҵ� %d ������\n", (int)cbb.size());
				for (int i = 0; i < cbb.size(); i++)
				{    //�ҵ�����������ٵ���box����Ƚ�Զ���ࣨ�������⵽��box������������������ƶȱȸ�������Ҫ��  
					if (bbOverlap(tbb, cbb[i]) < 0.8 && cconf[i] > tconf){  //  Get index of a clusters that is far from tracker and are more confident than the tracker
						confident_detections++;
						didx = i; //detection index
						bbnext = cbb[i];
						tconf = cconf[i];
					}
				}
				cout << "confident_detections="<<confident_detections << endl;
				lastconf = tconf;
				if (confident_detections == 1)
				{

						kcfbox = Rect2d(bbnext.x, bbnext.y, bbnext.width, bbnext.height);
							kcf = Tracker::create("KCF");
							kcf->init(frame, kcfbox);
							cout << "reinintialize tracker" << endl;
							lastvalid = true;
				}
				//���ֻ��һ����������������box����ô�������Ŀ��box�����³�ʼ����������Ҳ�����ü�����Ľ��ȥ������������

				//if (confident_detections == 1)//���ٵ��˵�û����
				//{                                //if there is ONE such a cluster, re-initialize the tracker
				//	printf("Found a better match..reinitializing tracking\n");
				//	bbnext = cbb[didx];
				//	if (tm)
				//	{
				//		float conf;
				//		Mat pattern;
				//		Scalar mean, stdev;
				//		bbnext = bbnext&Rect(0, 0, img2.cols, img2.rows);
				//		getPattern(img2(bbnext), pattern, mean, stdev);
				//		vector<int> isin;
				//		float dummy;
				//		//����ͼ��Ƭpattern������ģ��M�ı������ƶ�  
				//		classifier.NNConf(pattern, isin, dummy, conf); //Conservative Similarity tconf����csconf ������ڷ�������Conservative Similarity��5.2����Ϊ����Ŀ��ĵ÷ּ��ɣ�����Ҫ������÷ֺͼ�������бȽ�
				//		//�������ƶȴ�����ֵ��������������Ч 
				//		if (conf > classifier.thr_nn_valid)//���õĴ��ں�Ŀ��ģ������
				//		{
				//			lastboxfound = true;//�ж��켣�Ƿ���Ч���Ӷ������Ƿ�Ҫ��������������־λtvalid������5.6.2 P-Expert�� 

				//		}
				//		else
				//		{
				//			lastboxfound = true;
				//			//bbnext = Rect((int)Kalman.centerpt.x, (int)Kalman.centerpt.y, lastbox.width, lastbox.height);
				//			bbnext = lastbox;
				//		}

				//		kcfbox = Rect2d(bbnext.x, bbnext.y, bbnext.width, bbnext.height);
				//		kcf = Tracker::create("KCF");
				//		kcf->init(frame, kcfbox);
				//	}
				//	lastconf = cconf[didx];
				//	lastvalid = false;
		
				//}
				//else//confident_detections != 1
				//{
				//	// printf("�ҵ�%d�����ŶȽϸߵľ��� \n", confident_detections);
				//	int cx = 0, cy = 0, cw = 0, ch = 0;
				//	int close_detections = 0;
				//	for (int i = 0; i < dbb.size(); i++)
				//	{ //�ҵ��������⵽��box�������Ԥ�⵽��box����ܽ����ص��ȴ���0.7����box����������ʹ�С�����ۼ�  
				//		if (bbOverlap(tbb, dbb[i]) > 0.7)
				//		{ // Get mean of close detections
				//			cx += dbb[i].x;
				//			cy += dbb[i].y;
				//			cw += dbb[i].width;
				//			ch += dbb[i].height;
				//			close_detections++; //��¼�����box�ĸ���  
				//			// printf("��������ص��ȴ���0.7�ļ��������: %d %d %d %d\n", dbb[i].x, dbb[i].y, dbb[i].width, dbb[i].height);
				//		}
				//	}
				//	if (close_detections > 1)
				//	{//Ŀ��bounding box�����Ǹ�������Ȩֵ�ϴ�  close_detectionsһ���10С
				//		//bbnext.x = cvRound((float)(10 * tbb.x + cx) / (float)(10 + close_detections));   // weighted average trackers trajectory with the close detections
				//		//bbnext.y = cvRound((float)(10 * tbb.y + cy) / (float)(10 + close_detections));
				//		//bbnext.width = cvRound((float)(10 * tbb.width + cw) / (float)(10 + close_detections));
				//		//bbnext.height = cvRound((float)(10 * tbb.height + ch) / (float)(10 + close_detections));
				//		// printf("Tracker bb: %d %d %d %d\n", tbb.x, tbb.y, tbb.width, tbb.height);
				//		// printf("Average bb: %d %d %d %d\n", bbnext.x, bbnext.y, bbnext.width, bbnext.height);
				//		//  printf(" %d��Ȩ�ؼ����������������ں�\n", close_detections);
				//		/*
				//		kcfbox = Rect2d(bbnext.x, bbnext.y, bbnext.width, bbnext.height);
				//		kcf = Tracker::create("KCF");
				//		kcf->init(frame, kcfbox);
				//		*/
				//		lastvalid = false;
				//	}
				//	else
				//	{
				//		// printf("%d close detections were found\n", close_detections);

				//	}
				//}
			}
			else
			{
				
				lastconf = tconf;//��ʾ������ƶȵ���ֵ
				lastvalid = true;//��ʾ�������ƶȵ���ֵ
				
				/* lastconf = 1;
				 lastvalid = false;
				 lastboxfound = true;*/
				 //cout << "û�м�⵽����Ŀ�꣡������ʹ�ø���������" << endl;
			}
		}

		else{ //   If NOT tracking
			printf("Not tracking..\n");
			lastboxfound = false;
			lastvalid = false;
			if (detected)
			{  //���������û�и��ٵ�Ŀ�꣬���Ǽ������⵽��һЩ���ܵ�Ŀ��box����ôͬ��������о��࣬��ֻ�Ǽ򵥵�  
				//�������cbb[0]��Ϊ�µĸ���Ŀ��box�����Ƚ����ƶ��ˣ������������Ѿ��ź����ˣ����������³�ʼ�������� //  and detector is defined
				clusterConf(dbb, dconf, cbb, cconf);   //  cluster detections
				// printf("Found %d clusters\n",(int)cbb.size());
				if (cconf.size() == 1);

				{
					bbnext = cbb[0];
					lastconf = cconf[0];
					printf("Confident detection..reinitializing tracker\n");
					lastboxfound = true;
				}

			}
		}
		t2 = clock();
		// cout << "�ۺ�ģ���ʱ" << t2 - t1 << "ms" << endl;
		lastbox = bbnext;
		if (tm)
		{
			if (!tracked&&lastboxfound)
			{
				cout << "lx12345566" << endl;
				kcfbox = Rect2d(bbnext.x, bbnext.y, bbnext.width, bbnext.height);
				kcf = Tracker::create("KCF");
				kcf->init(frame, kcfbox);
			}
		}
		/*
		if (!tracked)
		{
			cout <<" lx" << endl;
			//kcfbox = Rect2d(bbnext.x, bbnext.y, bbnext.width, bbnext.height);
			kcf = Tracker::create("KCF");
			kcf->init(frame, lastbox);
		}
		*/
		/* if (lastboxfound)
		   fprintf(bb_file,"%d,%d,%d,%d,%f\n",lastbox.x,lastbox.y,lastbox.br().x,lastbox.br().y,lastconf);
		   else
		   fprintf(bb_file,"NaN,NaN,NaN,NaN,NaN\n");*/
		t1 = clock();
		cout << lastvalid << endl;
		if (lastvalid && tl)
			learn(img2, tk);
		t2 = clock();
		// cout << "ѧϰģ���ʱ" << t2 - t1 << "ms"<<endl;
		Point2f currentpt;
		currentpt.x = (float)lastbox.x + (float)lastbox.width / 2.f;
		currentpt.y = (float)lastbox.y + (float)lastbox.height / 2.f;
		if (tm)//��������ɷ�ģ��
		{
			if (tracked)	//�������ģ��ɹ������õĸ��ٵ������£�������Ŀ�������ĵ�������
			{
				Mp1.tmupdate(trackpt1, trackpt2, currentpt);
				Mp2.tmupdate(trackpt1, trackpt2, currentpt);
			}
			else
			{
				Mp1.tmupdate(currentpt);
				Mp2.tmupdate(currentpt);
			}
		}
	}


void TLD::track(const Mat& img1, const Mat& img2,vector<Point2f>& points1,vector<Point2f>& points2,Point2f& pt1,Point2f& pt2){
  /*Inputs:
   * -current frame(img2), last frame(img1), last Bbox(bbox_f[0]).
   *Outputs:
   *- Confidence(tconf), Predicted bounding box(tbb),Validity(tvalid), points2 (for display purposes only)
   */
  //Generate points
	//����������㣨���Ȳ���������lastbox�й��������10*10=100�������㣬����points1  
  bbPoints(points1,lastbox);	//��ʼ��ʱlatsbox=best_box
  if (points1.size()<1){
     // printf("BB= %d %d %d %d,�������ٵ�û������\n",lastbox.x,lastbox.y,lastbox.width,lastbox.height);
      tvalid=false;
      tracked=false;
      return;
  }
  vector<Point2f> points = points1;

  //Frame-to-frame tracking with forward-backward error cheking  
  //trackf2f������ɣ����١�����FB error��ƥ�����ƶ�sim��Ȼ��ɸѡ�� FB_error[i] <= median(FB_error) ��   
  //sim_error[i] > median(sim_error) �������㣨���ٽ�����õ������㣩��ʣ�µ��ǲ���50%��������,�ɹ����ص�true��ʧ�ܷ��ص���flase  
  tracked = tracker.trackf2f(img1,img2,points,points2,pt1,pt2);
  if (tracked)
  {
      //Bounding box prediction
	  //����ʣ�µ��ⲻ��һ��ĸ��ٵ�������Ԥ��bounding box�ڵ�ǰ֡��λ�úʹ�С tbb 
      bbPredict(points,points2,lastbox,tbb);
	  //����ʧ�ܼ�⣺���FB error����ֵ����10�����أ�����ֵ��������Ԥ�⵽�ĵ�ǰbox��λ���Ƴ�ͼ����  
	  //��Ϊ���ٴ��󣬴�ʱ������bounding box��Rect::br()���ص������½ǵ�����  
	  //getFB()���ص���FB error����ֵ  
      if (tracker.getFB()>10 || tbb.x>img2.cols ||  tbb.y>img2.rows || tbb.br().x < 1 || tbb.br().y <1){
          tvalid =false; //too unstable prediction or bounding box out of image
          tracked = false;
         // printf("̫�����׵Ĺ���Ԥ��FB error=%f\n",tracker.getFB());//Too unstable predictions FB error=%f\n
          return;
      }
      //Estimate Confidence and Validity
	  //��������ȷ�ŶȺ���Ч��  
      Mat pattern;
      Scalar mean, stdev;
      BoundingBox bb;
      bb.x = max(tbb.x,0);
      bb.y = max(tbb.y,0);
      bb.width = min(min(img2.cols-tbb.x,tbb.width),min(tbb.width,tbb.br().x));//����Ŀ��ֻ��һ��������Ұ�У����������Ƶ����
      bb.height = min(min(img2.rows-tbb.y,tbb.height),min(tbb.height,tbb.br().y));
	  //��һ��img2(bb)��Ӧ��patch��size��������patch_size = 15*15��������pattern 
      getPattern(img2(bb),pattern,mean,stdev);
      vector<int> isin;
      float dummy,conf;
	  //����ͼ��Ƭpattern������ģ��M�ı������ƶ�  
      classifier.NNConf(pattern,isin,conf,dummy,tconf); //Conservative Similarity tconf����csconf ������ڷ�������Conservative Similarity��5.2����Ϊ����Ŀ��ĵ÷ּ��ɣ�����Ҫ������÷ֺͼ�������бȽϡ�
      tvalid = lastvalid;
	  //�������ƶȴ�����ֵ��������������Ч 
      if (tconf>classifier.thr_nn_valid){
          tvalid =true;//�ж��켣�Ƿ���Ч���Ӷ������Ƿ�Ҫ��������������־λtvalid������5.6.2 P-Expert�� 
      }
  }
  else
    printf("û�е㱻���ٵ�\n");

}

//����������㣬box��10*10=100��������  
void TLD::bbPoints(vector<cv::Point2f>& points,const BoundingBox& bb){
  int max_pts=10;
  int margin_h=0;//�����߽�  
  int margin_v=0;
  int stepx = ceil((bb.width-2*margin_h)/max_pts);//ceil���ش��ڻ��ߵ���ָ�����ʽ����С����  
  int stepy = ceil((bb.height-2*margin_v)/max_pts);
  //����������㣬box��10*10=100��������  
  for (int y=bb.y+margin_v;y<bb.y+bb.height-margin_v;y+=stepy){
      for (int x=bb.x+margin_h;x<bb.x+bb.width-margin_h;x+=stepx){
          points.push_back(Point2f(x,y));
      }
  }
}

//����ʣ�µ��ⲻ��һ��ĸ��ٵ�������Ԥ��bounding box�ڵ�ǰ֡��λ�úʹ�С  
void TLD::bbPredict(const vector<cv::Point2f>& points1,const vector<cv::Point2f>& points2,
                    const BoundingBox& bb1,BoundingBox& bb2)    
{
  int npoints = (int)points1.size();
  vector<float> xoff(npoints);//λ��  
  vector<float> yoff(npoints);
 // printf("�������ٵ� : %d����\n",npoints);
  for (int i=0;i<npoints;i++){//����ÿ������������֮֡���λ�� 
      xoff[i]=points2[i].x-points1[i].x;
      yoff[i]=points2[i].y-points1[i].y;
  }
  float dx = median(xoff);
  float dy = median(yoff);
  float s;
  //����bounding box�߶�scale�ı仯��ͨ������ ��ǰ�������໥��ľ��� �� ��ǰ����һ֡���������໥��ľ��� ��  
  //��ֵ���Ա�ֵ����ֵ��Ϊ�߶ȵı仯����  
  if (npoints>1){
      vector<float> d;
      d.reserve(npoints*(npoints-1)/2);
      for (int i=0;i<npoints;i++){
          for (int j=i+1;j<npoints;j++){
			  //���� ��ǰ�������໥��ľ��� �� ��ǰ����һ֡���������໥��ľ��� �ı�ֵ��λ���þ���ֵ��  
              d.push_back(norm(points2[i]-points2[j])/norm(points1[i]-points1[j]));
          }
      }
      s = median(d);
  }
  else {
      s = 1.0;
  }
  float s1 = 0.5*(s-1)*bb1.width;// top-left �����ƫ��(s1,s2) 
  float s2 = 0.5*(s-1)*bb1.height;
  //printf("�������ٳ߶ȱ仯s= %f ��ƫ��s1= %f��ƫ�� s2= %f \n",s,s1,s2);
  //�õ���ǰbounding box��λ�����С��Ϣ  
  //��ǰbox��x���� = ǰһ֡box��x���� + ȫ��������λ�Ƶ���ֵ�������Ϊbox�ƶ����Ƶ�λ�ƣ� - ��ǰbox���һ��
  bb2.x = round( bb1.x + dx -s1);
  bb2.y = round( bb1.y + dy -s2);
  bb2.width = round(bb1.width*s);
  bb2.height = round(bb1.height*s);
  //printf("������Ԥ�⵽�ķ��� bb: %d %d %d %d\n",bb2.x,bb2.y,bb2.width,bb2.height);
  //changeGrid(bb2);
}
void TLD::changeGrid(BoundingBox& tobb)
{
	BoundingBox tb;
	gridoftracker.reserve(grid.size());
	for (int i = 0; i < grid.size(); i++)
	{
		tb = grid[i];
		tb.x = min(max(grid[i].x + tobb.x - lastbox.x,1),currentframe.cols);
		tb.y = max(min(grid[i].x + tobb.y - lastbox.y,currentframe.rows),1);
		gridoftracker.push_back(tb);
	}
	
}
void TLD::detect(const cv::Mat& frame,bool lastboxfound,bool tk,bool tm,float spdirection,float czdirection){
  //cleaning
  dbb.clear();
  dconf.clear();
  dt.bb.clear();//���Ľ����һ��Ŀ��һ��bounding box 
  kalman_boxes.clear();
  double t = (double)getTickCount();
  
  Mat img(frame.rows,frame.cols,CV_8U);
  integral(frame,iisum,iisqsum);
  GaussianBlur(frame,img,Size(9,9),1.5);
  int numtrees = classifier.getNumStructs();
  float fern_th = classifier.getFernTh();//getFernTh()����thr_fern; ���Ϸ������ķ�����ֵ,�����ֵ�Ǿ���ѵ����
  vector <int> ferns(10);
  float conf;
  int a=0;
  Mat patch;
  
  //����������ģ��һ��������ģ�飬���û���ͼ����ÿ������ⴰ�ڵķ���������var��ֵ��Ŀ��patch�����50%���ģ�  
  //����Ϊ�京��ǰ��Ŀ�� 
  RNG rng;
  int count = 0;
  double fangcha = 0;
 /* //cout << "grid.size=" << grid.size() << endl;
  float ratofscal = tbb.width*1.0f / (lastbox.width*1.0f);//tbb��track�Ժ��Ŀ���lastbox��֮ǰ��
  int scalidx=0;
  float absdetasc = abs(ratofscal - SCALES[0]);//����ֵ
  for (int i = 0; i < gridscalnum.size()-1; i++)	//gridscalnum�����зŵ��ǵ�sc���߶����ж��ٸ�ɨ�贰�ڣ���size���ǳ߶������������ô����
  {
	  if (abs(ratofscal - SCALES[i + 1]) <absdetasc)	//��Ӧ����Ϊ���ҳ���ӽ��ڸô����ŵĳ߶Ȱɡ���
	  {
		  absdetasc = abs(ratofscal - SCALES[i + 1]);
		  //cout << "abs(ratofscal - SCALES[i])=" << abs(ratofscal - SCALES[i]) << endl;
		  scalidx = i+1;
	  }
		  
  }
  cout << "ratio of scale ������ǰ����֡�߶ȱ仯=" << ratofscal << endl;
  cout << "�ߴ�scalidx��" << scalidx << endl;
 
  
  for (; numj <= scalidx-2; numj++)	//numl����������ŵĳ߶�֮ǰ������ɨ�贰����
  {
	  numl += gridscalnum[numj];
  }
  if (scalidx <= (gridscalnum.size()-1)/2)
  {
	  numr = numl + gridscalnum[numj]+gridscalnum[numj+1]+gridscalnum[numj+2]+gridscalnum[numj+3];
  }
  else
	  numr = grid.size();
  cout << "�ӵ�num=" << numl << "��grid��ʼ�����" << endl;
  */
  clock_t t1 = clock();
  if (lastboxfound&&tk)
  {
	  Point2f measurept ;
	  Point2f predictpt ;
	  measurept.x = (float)lastbox.x +(float) lastbox.width / 2.f;
	  measurept.y = (float)lastbox.y +(float) lastbox.height / 2.f;
	  Kalman.kalmanpredict(measurept, predictpt);
	// Mp1.tmupdate(measurept);
	// Mp2.tmupdate(measurept);
	 int varr =round(predictpt.x + 3.f*(float)lastbox.width/2.f);
	 int varu =round(predictpt.y + 3.f*(float)lastbox.height/2.f) ;
	 int varl =round(predictpt.x - 3.f*(float)lastbox.width/2.f);
	 int vard =round( predictpt.y - 3.f*(float)lastbox.height/2.f);
	 //printf("%d\t%d\t%d\t%d\n", varr, varl, varu, vard);
	 /*
	 if (tm)
	 {
		 
		 if (spdirection == 1.f)
			 varl =round(measurept.x - (float)lastbox.width / 2.f);
		 if (spdirection == 0.f)
			 varr = round(measurept.x + (float)lastbox.width / 2.f);
		 if (czdirection == 1.f)
			 vard = round(measurept.y - (float)lastbox.height / 2.f);
		 if (czdirection == 0.f)
			 varu =round(measurept.y +(float) lastbox.height / 2.f);
			 
	 }
	 */
	 
	 for (int i = 0; i < grid.size(); i++)
	 {
		 if (grid[i].x > varr/*predictpt.x +2 * lastbox.width*/ || grid[i].y >varu/* predictpt.y + 2 * lastbox.height*/ || grid[i].br().x < varl/*predictpt.x - 2 * lastbox.width*/ || grid[i].br().y < vard/*predictpt.y - 2 * lastbox.height*/);
		 else
		 {
			 kalman_boxes.push_back(i);
			 fangcha = getVar(grid[i], iisum, iisqsum);
			 if (fangcha >= var)////��һ�أ����� best_box�ı�׼��*0.5
			 {//����ÿһ��ɨ�贰�ڵķ���  
				 a++;
				 //����������ģ��������Ϸ��������ģ��  
				 patch = img(grid[i]);
				 classifier.getFeatures(patch, grid[i].sidx, ferns);//�õ���patch������13λ�Ķ����ƴ��룩  
				 conf = classifier.measure_forest(ferns);//���������ֵ��Ӧ�ĺ�������ۼ�ֵ  
				 tmp.conf[i] = conf;
				 tmp.patt[i] = ferns;
				 //������Ϸ������ĺ�����ʵ�ƽ��ֵ������ֵfern_th����ѵ���õ���������Ϊ����ǰ��Ŀ��  
				 if (conf > numtrees*fern_th){
					 /* printf("conf=%f,fern=", conf);
					 for (int i = 0; i < 10; i++)
					 {
					 cout << ferns[i]<<" ";
					 }
					 cout << endl;*/
					 dt.bb.push_back(i);//��ͨ�������������ģ���ɨ�贰�ڼ�¼��detect structure��  
				 }
			 }
			 else
				 tmp.conf[i] = 0.0;
		 }
	 }
  }
  else{
	  numl = 0;
	  numr = grid.size();
	  for (int i = numl; i < numr; i++)
	  {
		 
			  fangcha = getVar(grid[i], iisum, iisqsum);
			  if (fangcha >= var)////��һ�أ����� best_box�ı�׼��*0.5
			  {//����ÿһ��ɨ�贰�ڵķ���  
				  a++;
				  //����������ģ��������Ϸ��������ģ��  
				  patch = img(grid[i]);
				  classifier.getFeatures(patch, grid[i].sidx, ferns);//�õ���patch������13λ�Ķ����ƴ��룩  
				  conf = classifier.measure_forest(ferns);//���������ֵ��Ӧ�ĺ�������ۼ�ֵ  
				  tmp.conf[i] = conf;
				  tmp.patt[i] = ferns;
				  //������Ϸ������ĺ�����ʵ�ƽ��ֵ������ֵfern_th����ѵ���õ���������Ϊ����ǰ��Ŀ��  
				  if (conf > numtrees*fern_th){
					 
					  dt.bb.push_back(i);//��ͨ�������������ģ���ɨ�贰�ڼ�¼��detect structure��  
				  }
			  }
			  else
				  tmp.conf[i] = 0.0;

	  }
  }
	  
  clock_t t2 = clock();
  //cout << " grid������ķ�������������ɭ�ַ�������ʱ" << t2 - t1 << "ms" << endl;
  int detections = dt.bb.size();
  //printf("%d Bounding boxes ͨ���˷��������\n",a);
 // printf("%d Initial detection ͨ�������ɭ�ַ�����\n",detections);
  //���ͨ�������������ģ���ɨ�贰��������100������ֻȡ������ʴ��ǰ100�� 
  if (detections>15){
      nth_element(dt.bb.begin(),dt.bb.begin()+15,dt.bb.end(),CComparator(tmp.conf));
      dt.bb.resize(15);
      detections=15;
  }
 // printf("�����Ĵ�����Ϊ%d", detections);
//  for (int i=0;i<detections;i++){
//        drawBox(img,grid[dt.bb[i]]);
//    }
//  imshow("detections",img);
  if (detections==0){
	  //printf("û��ͼ���ͨ�����ɭ��");
        detected=false;
        return;
      }
 // printf("Fern detector made %d detections ",detections);
 // t=(double)getTickCount()-t;
 // printf("in %gms\n", t*1000/getTickFrequency());
                                                                       //  Initialize detection structure
  dt.patt = vector<vector<int> >(detections,vector<int>(10,0));        //  Corresponding codes of the Ensemble Classifier
  dt.conf1 = vector<float>(detections);                                //  Relative Similarity (for final nearest neighbour classifier)
  dt.conf2 =vector<float>(detections);                                 //  Conservative Similarity (for integration with tracker)
  dt.isin = vector<vector<int> >(detections,vector<int>(3,-1));        //  Detected (isin=1) or rejected (isin=0) by nearest neighbour classifier
  dt.patch = vector<Mat>(detections,Mat(patch_size,patch_size,CV_32F));//  Corresponding patches
  int idx;
  Scalar mean, stdev;
  float dummy;
  //����������ģ����������ڷ��������ģ�� 
  t1 = clock();
  float nn_th = classifier.getNNTh();
  for (int i=0;i<detections;i++){                                         //  for every remaining detection
      idx=dt.bb[i];                                                       //  Get the detected bounding box index
	  patch = frame(grid[idx]);
      getPattern(patch,dt.patch[i],mean,stdev);                //  Get pattern within bounding box
      classifier.NNConf(dt.patch[i],dt.isin[i],dt.conf1[i],dummy,dt.conf2[i]);  //  Evaluate nearest neighbour classifier
      dt.patt[i]=tmp.patt[idx];
      //printf("Testing feature %d, conf:%f isin:(%d|%d|%d)\n",i,dt.conf1[i],dt.isin[i][0],dt.isin[i][1],dt.isin[i][2]);
      if (dt.conf1[i]>nn_th){                                               //  idx = dt.conf1 > tld.model.thr_nn; % get all indexes that made it through the nearest neighbour
          dbb.push_back(grid[idx]);                                         //  BB    = dt.bb(:,idx); % bounding boxes
          dconf.push_back(dt.conf2[i]);                                     //  Conf  = dt.conf2(:,idx); % conservative confidences
      }
  }      
  t2 = clock();
  Mat test;
  //cout << "�����������ڷ�������ʱ" << t2 - t1 << "ms" << endl;
  //��ӡ��⵽�Ŀ��ܴ���Ŀ���ɨ�贰����������ͨ����������������ģ� //  end
  if (dbb.size()>0){
      //printf("����ҵ� %d �������ƥ���.\n",(int)dbb.size());
      detected=true;
	  for (int i = 0; i < dbb.size(); i++)
	  {
		  test = frame(dbb[i]);
	  }
  }
  else{
      printf("û�ҵ������ƥ���.\n");
      detected=false;
  }
}

void TLD::evaluate(){
}

void TLD::learn(const Mat& img,bool tk){
  //printf("[Learning] ");
  ///Check consistency
  BoundingBox bb;
  bb.x = max(lastbox.x,0);
  bb.y = max(lastbox.y,0);
  bb.width = min(min(img.cols-lastbox.x,lastbox.width),min(lastbox.width,lastbox.br().x));
  bb.height = min(min(img.rows-lastbox.y,lastbox.height),min(lastbox.height,lastbox.br().y));
  Scalar mean, stdev;
  Mat pattern;
  //��һ��img(bb)��Ӧ��patch��size��������patch_size = 15*15��������pattern  
  getPattern(img(bb),pattern,mean,stdev);
  vector<int> isin;
  float dummy, conf,mconf;
  classifier.NNConf(pattern,isin,conf,dummy,mconf);
  /*if (conf > 0.5&&conf < 0.65)
  {
	  cout << "123" << endl;
  }*/
  if (conf<0.5) { //������ƶ�̫С�ˣ��Ͳ�ѵ�� 
      //printf("rsconf̫С����ѵ����\n");
      lastvalid =false;
      return;
  }
  if (pow(stdev.val[0],2)<var){//�������̫С�ˣ�Ҳ��ѵ�� 
      printf("����̫С����ѵ����\n");
      lastvalid=false;
      return;
  }
  if(isin[2]==1){//�������ʶ��Ϊ��������Ҳ��ѵ��  
      //printf("ͼ������ڸ���������ѵ����\n");
      lastvalid=false;
      return;
  }
/// Data generation
  //clock_t t1 = clock();
  clock_t t1 = clock();
  for (int i=0;i<grid.size();i++){
      grid[i].overlap = bbOverlap(lastbox,grid[i]);
  }
  //clock_t t2 = clock();
  //cout << "overlap Time is " << t2 - t1 << "ms" << endl;
 // for (int i = 0;i<dt.bb.size())
  vector<pair<vector<int>,int> > fern_examples;
  good_boxes.clear();
  bad_boxes.clear();
 
// printf("%d\t%d\n", grid.size(), kalman_boxes.size());
 if(tk)
	 getOverlappingBoxes(lastbox, num_closest_update,tk);//����kalman�˲����������������򣬼���������������
 else
  getOverlappingBoxes(lastbox,num_closest_update);
// classifier.pmodel = img(best_box);
 // printf("%d\t%d\n", good_boxes.size(), bad_boxes.size());
  if (good_boxes.size()>0)
    generatePositiveData(img,num_warps_update);//�÷���ģ�Ͳ����������������ڵ�һ֡�ķ�������ֻ����10*10=100����  
  else{
    lastvalid = false;
    printf("No good boxes..Not training");
    return;
  }
  fern_examples.reserve(pX.size()+bad_boxes.size());
  fern_examples.assign(pX.begin(),pX.end());
  int idx;
  for (int i=0;i<bad_boxes.size();i++){
      idx=bad_boxes[i];
	  if (tmp.conf[idx] >= 1){ //���븺���������ƶȴ���1�������ƶȲ��ǳ���0��1֮����//��detect�����п������temp.conf�Ǻ��������ӵõ��ģ�����>=1������Щ�������Ƚ����׻���
          fern_examples.push_back(make_pair(tmp.patt[idx],0));
      }
  }
  //����ڷ�����
  vector<Mat> nn_examples;
  vector<Mat>nn_images;
  nn_examples.reserve(dt.bb.size()+1);
  nn_examples.push_back(pEx);
  nn_images.reserve(dt.bb.size() + 1);
  nn_images.push_back(img(lastbox));
  for (int i=0;i<dt.bb.size();i++){
      idx = dt.bb[i];
	  if (bbOverlap(lastbox, grid[idx]) < 0.1)
	  {
		  nn_examples.push_back(dt.patch[i]);
		  nn_images.push_back(img(grid[idx]));
	  }
  }
  /// Classifiers update
  /// ������ѵ��  
 // printf("%d\t%d\n", fern_examples.size(), nn_examples.size());
  classifier.trainF(fern_examples,2);
  clock_t t2 = clock();
  //cout << "generate good and bad boxes time is " << t2 - t1 << "ms" << endl;
  classifier.trainNN(nn_examples,nn_images);
 // classifier.show();//���������⣨����ģ�ͣ�������������������ʾ�ڴ�����  
  //trainTh(img(lastbox));
  //cout << "learning"<< endl;
  cout << "Pex�ĸ���Ϊ" << classifier.pEx.size() << endl;
  cout << "Nex�ĸ���Ϊ" << classifier.nEx.size() << endl;
}

void TLD::buildGrid(const cv::Mat& img, const cv::Rect& box){
  const float SHIFT = 0.1;
 /* const float SCALES[] = {0.16151,0.19381,0.23257,0.27908,0.33490,0.40188,0.48225,
						  0.57870,0.69444,0.83333,1,1.20000,1.44000,1.72800,
						  2.07360,2.48832,2.98598,3.58318,4.29982,5.15978,6.19174};*/
 /* const float SCALES[] =  {0.90909,0.82645,0.75131,0.68301,0.62092,0.56447,0.51316,0.46651,0.42410,0.38554,1,
						   1.1,1.21,1.331,1.4641,1.61051,1.77156,1.94871,2.14359,2.35794,2.59374
						   };*/
  /*const float SCALES[] = {  0.33490, 0.40188, 0.48225,
	  0.57870, 0.69444, 0.83333, 1, 1.20000, 1.44000, 1.72800,
	  2.07360, 2.48832, 2.98598, 3.58318, 4.29982, 5.15978, 6.19174 };*/
 
  int width, height, min_bb_side;
  //Rect bbox;
  BoundingBox bbox;
  Size scale;
  int sc=0;
  for (int s=0;s<21;s++)
  {
    width = round(box.width*SCALES[s]);//round��������������Ϊ����
    height = round(box.height*SCALES[s]);
    min_bb_side = min(height,width);
    if (min_bb_side < min_win || width > img.cols || height > img.rows)
      continue;
    scale.width = width;
    scale.height = height;
    scales.push_back(scale);
	/*for (int y=1;y<img.rows-height;y+=round(SHIFT*min_bb_side))//����ԭʼ�����ѭ����ɨ���������ͼ��Ч�ʱȽϵ�
	{
	for (int x=1;x<img.cols-width;x+=round(SHIFT*min_bb_side))
	{
	bbox.x = x;
	bbox.y = y;
	bbox.width = width;
	bbox.height = height;
	bbox.overlap = bbOverlap(bbox,BoundingBox(box));
	bbox.sidx = sc;
	grid.push_back(bbox);
	}
	}*/
	Rect area;//�Ľ�����㷨���ǻ������������boundingbox����Ŀ�����ڵ�ͼ��飬ɨ�贰�ڰ���Ŀ��ͼ��飬������Ч��
	int num = 0;
	//area.x = max(1,box.x-3*box.width);
	//area.y = max(1, box.y - 3*box.height);
	area.x = 1;
	area.y = 1;
	for (int y = area.y; y < img.rows-height/*min(img.rows, box.y +  4* box.height)-height*/; y += round(SHIFT*min_bb_side))
	for (int x = area.x; x <img.cols-width/*min(img.cols, box.x+4* box.width) - width*/; x += round(SHIFT*min_bb_side))
	{
		bbox.x = x;
		bbox.y = y;
		bbox.width = width;
		bbox.height = height;
		bbox.overlap = bbOverlap(bbox, BoundingBox(box));//���㵱ǰͼ�����Ŀ��ͼ�����ص���
		bbox.sidx = sc;
		grid.push_back(bbox);
		num++;
	}
	gridscalnum.push_back(num);//gridscalnum��������зŵ��ǵ�sc���߶��ж��ٸ�ɨ�贰��
    sc++;
  }
}
int max(int a,int b)
{
	if (a > b)
		return a;
	else
		return b;
}

float TLD::bbOverlap(const BoundingBox& box1,const BoundingBox& box2)//�ص��ȼ�����ͼ����ص�������������
{
  if (box1.x > box2.x+box2.width) { return 0.0; }
  if (box1.y > box2.y+box2.height) { return 0.0; }
  if (box1.x+box1.width < box2.x) { return 0.0; }
  if (box1.y+box1.height < box2.y) { return 0.0; }

  float colInt =  min(box1.x+box1.width,box2.x+box2.width) - max(box1.x, box2.x);
  float rowInt =  min(box1.y+box1.height,box2.y+box2.height) - max(box1.y,box2.y);

  float intersection = colInt * rowInt;
  float area1 = box1.width*box1.height;
  float area2 = box2.width*box2.height;
  return intersection / (area1 + area2 - intersection);
}

void TLD::getOverlappingBoxes(const cv::Rect& box1, int num_closest) {
	float max_overlap = 0;
	// vector<int>::iterator it;
	
		for (int i = 0; i < grid.size(); i++)
		{
			if (grid[i].overlap > max_overlap) //�ҵ�����ص��ʵ�ͼ���
			{
				max_overlap = grid[i].overlap;
				best_box = grid[i];
			}
			if (grid[i].overlap > 0.7)//�ص��ʴ���0.6�Ļ��ͷŽ�goodboxes��������У�ע��Ž�ȥ��ֻ�����ͼ���ı��
			{
				good_boxes.push_back(i);
			}
			else if (grid[i].overlap < bad_overlap) {
				bad_boxes.push_back(i);
			}
		}
		//Get the best num_closest (10) boxes and puts them in good_boxes
		if (good_boxes.size() > num_closest) {
			//for (it = good_boxes.begin(); it != good_boxes.end(); it++)
				//cout << *it << ends;
			//cout << endl;
			std::nth_element(good_boxes.begin(), good_boxes.begin() + num_closest, good_boxes.end(), OComparator(grid));
			//����һ�����������㷨��goodboxes�зŵ�����Щͼ�����ţ�ͨ����Щ��űȽϵ�����Щ�����grid�д����ֵ������������ָ
			//ֻע�صڶ�������������������ȷ�ľͿ��ԣ�֮ǰ����֮����ô�ŵ�����ν��������Ч�ʣ�ͨ����Ļ����������Եؿ���������
			//for (it = good_boxes.begin(); it != good_boxes.end(); it++)
				//cout << *it << ends;
			//cout << endl;
			good_boxes.resize(num_closest);
			//	for (it = good_boxes.begin(); it != good_boxes.end(); it++)
					//cout << *it << ends;
				//cout << endl;
		}
		getBBHull();

}
void TLD::getOverlappingBoxes(const cv::Rect& box1, int num_closest,bool tk) {
	float max_overlap = 0;
	// vector<int>::iterator it;
	//printf("%d\t%d\n", grid.size(), kalman_boxes.size());
	int idx = 0;
	for (int i = 0; i < kalman_boxes.size(); i++)
	{
		//printf("%d\t%d", grid.size(), kalman_boxes.size());
		idx = kalman_boxes[i];
		//printf("%d\n", idx);
		if (idx<0)
			break;
		if (grid[idx].overlap > max_overlap) //�ҵ�����ص��ʵ�ͼ���
		{
			max_overlap = grid[idx].overlap;
			best_box = grid[idx];
		}
		if (grid[idx].overlap > 0.7)//�ص��ʴ���0.6�Ļ��ͷŽ�goodboxes��������У�ע��Ž�ȥ��ֻ�����ͼ���ı��
		{
			good_boxes.push_back(idx);
		}
		else if (grid[idx].overlap < bad_overlap) {
			bad_boxes.push_back(idx);
		}
	}
	//Get the best num_closest (10) boxes and puts them in good_boxes
	if (good_boxes.size() > num_closest) {
		//for (it = good_boxes.begin(); it != good_boxes.end(); it++)
		//cout << *it << ends;
		//cout << endl;
		std::nth_element(good_boxes.begin(), good_boxes.begin() + num_closest, good_boxes.end(), OComparator(grid));
		//����һ�����������㷨��goodboxes�зŵ�����Щͼ�����ţ�ͨ����Щ��űȽϵ�����Щ�����grid�д����ֵ������������ָ
		//ֻע�صڶ�������������������ȷ�ľͿ��ԣ�֮ǰ����֮����ô�ŵ�����ν��������Ч�ʣ�ͨ����Ļ����������Եؿ���������
		//for (it = good_boxes.begin(); it != good_boxes.end(); it++)
		//cout << *it << ends;
		//cout << endl;
		good_boxes.resize(num_closest);
		//	for (it = good_boxes.begin(); it != good_boxes.end(); it++)
		//cout << *it << ends;
		//cout << endl;
	}
	getBBHull();

}
//���ʵ������10��goodboxes�Ĳ����ı߿�
void TLD::getBBHull(){
  int x1=INT_MAX, x2=0;//��ʼ��
  int y1=INT_MAX, y2=0;
  int idx;
  for (int i=0;i<good_boxes.size();i++){
      idx= good_boxes[i];
      x1=min(grid[idx].x,x1);
      y1=min(grid[idx].y,y1);
      x2=max(grid[idx].x+grid[idx].width,x2);
      y2=max(grid[idx].y+grid[idx].height,y2);
  }
  bbhull.x = x1;		//bbhull���boundingbox���д��������ѡ����goodboxes�ı߽磬�����ǵĲ����ı߿�
  bbhull.y = y1;
  bbhull.width = x2-x1;
  bbhull.height = y2 -y1;
}

bool bbcomp(const BoundingBox& b1,const BoundingBox& b2){
  TLD t;
    if (t.bbOverlap(b1,b2)<0.5)
      return false;
    else
      return true;
}
int TLD::clusterBB(const vector<BoundingBox>& dbb,vector<int>& indexes){
  //FIXME: Conditional jump or move depends on uninitialised value(s)
  const int c = dbb.size();
  //1. Build proximity matrix
  Mat D(c,c,CV_32F);
  float d;
  for (int i=0;i<c;i++){
      for (int j=i+1;j<c;j++){
        d = 1-bbOverlap(dbb[i],dbb[j]);
        D.at<float>(i,j) = d;
        D.at<float>(j,i) = d;
      }
  }
  //2. Initialize disjoint clustering
  float *L = new float[c - 1]; //Level
  int **nodes = new int *[c - 1];
  for (int i = 0; i < 2; i++)
	  nodes[i] = new int[c - 1];
  int *belongs = new int[c];
  //�ǵ��ں���ĩ�ͷŷ�����ڴ�
  delete[] L;
  L = NULL;
  for (int i = 0; i < 2; ++i)
  {
	  delete[] nodes[i];
	  nodes[i] = NULL;
  }
  delete[]nodes;
  nodes = NULL;
  delete[] belongs;
  belongs = NULL;
 int m=c;
 for (int i=0;i<c;i++){
    belongs[i]=i;
 }
 for (int it=0;it<c-1;it++){
 //3. Find nearest neighbor
     float min_d = 1;
     int node_a, node_b;
     for (int i=0;i<D.rows;i++){
         for (int j=i+1;j<D.cols;j++){
             if (D.at<float>(i,j)<min_d && belongs[i]!=belongs[j]){
                 min_d = D.at<float>(i,j);
                 node_a = i;
                 node_b = j;
             }
         }
     }
     if (min_d>0.5){
         int max_idx =0;
         bool visited;
         for (int j=0;j<c;j++){
             visited = false;
             for(int i=0;i<2*c-1;i++){
                 if (belongs[j]==i){
                     indexes[j]=max_idx;
                     visited = true;
                 }
             }
             if (visited)
               max_idx++;
         }
         return max_idx;
     }

 //4. Merge clusters and assign level
     L[m]=min_d;
     nodes[it][0] = belongs[node_a];
     nodes[it][1] = belongs[node_b];
     for (int k=0;k<c;k++){
         if (belongs[k]==belongs[node_a] || belongs[k]==belongs[node_b])
           belongs[k]=m;
     }
     m++;
 }
 return 1;

}

void TLD::clusterConf(const vector<BoundingBox>& dbb,const vector<float>& dconf,vector<BoundingBox>& cbb,vector<float>& cconf){
  int numbb =dbb.size();
  vector<int> T;
  float space_thr = 0.5;
  int c=1;
  switch (numbb){
  case 1:
    cbb=vector<BoundingBox>(1,dbb[0]);
    cconf=vector<float>(1,dconf[0]);
    return;
    break;
  case 2:
    T =vector<int>(2,0);
    if (1-bbOverlap(dbb[0],dbb[1])>space_thr){
      T[1]=1;
      c=2;
    }
    break;
  default:
    T = vector<int>(numbb,0);
    c = partition(dbb,T,(*bbcomp));
    //c = clusterBB(dbb,T);
    break;
  }
  cconf=vector<float>(c);
  cbb=vector<BoundingBox>(c);
 // printf("Cluster indexes: ");
  BoundingBox bx;
  for (int i=0;i<c;i++){
      float cnf=0;
      int N=0,mx=0,my=0,mw=0,mh=0;
      for (int j=0;j<T.size();j++){
          if (T[j]==i){
              //printf("%d ",i);
              cnf=cnf+dconf[j];
              mx=mx+dbb[j].x;
              my=my+dbb[j].y;
              mw=mw+dbb[j].width;
              mh=mh+dbb[j].height;
              N++;
          }
      }
      if (N>0){
          cconf[i]=cnf/N;
          bx.x=cvRound(mx/N);
          bx.y=cvRound(my/N);
          bx.width=cvRound(mw/N);
          bx.height=cvRound(mh/N);
          cbb[i]=bx;
      }
  }
  //printf("\n");
}
//void TLD::trainTh(Mat&img)
//{
//
//	Mat pattern;
//	Scalar mean, stdev;
//	getPattern(img, pattern, mean, stdev);
//	vector<int> isin;
//	float dummy;
//	float conf;
//	//����ͼ��Ƭpattern������ģ��M�ı������ƶ�  
//	classifier.NNConf1(pattern, isin, dummy, conf);
//	classifier.thr_nn_valid = conf;
//}
