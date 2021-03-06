#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "sp_segmenter/features.h"
#include "sp_segmenter/JHUDataParser.h"
#include "sp_segmenter/common.h"

std::map<std::string, int> model_name_map;
uchar color_label[11][3] = 
{ {0, 0, 0}, 
  {255, 0, 0},
  {0, 255, 0},
  {0, 0, 255},
  {255, 255, 0},
  {255, 0, 255},
  {0, 255, 255},
  {255, 128, 0},
  {255, 0, 128},
  {0, 128, 255},
  {128, 0, 255},
};   

std::vector<std::string> readMesh(std::string mesh_path, std::vector<ModelT> &model_set);
std::vector<poseT> readGT(std::string pose_path, std::string file_id);
std::vector<int> spGt(const pcl::PointCloud<PointT>::Ptr gt_cloud, const std::vector<pcl::PointCloud<PointT>::Ptr> segs);
void visualizeLabels(const pcl::PointCloud<PointLT>::Ptr label_cloud, pcl::visualization::PCLVisualizer::Ptr viewer, uchar colors[][3]);
std::vector<PR_ELEM> semanticPR(const pcl::PointCloud<PointLT>::Ptr gt_cloud, const pcl::PointCloud<PointLT>::Ptr label_cloud, int model_num);
pcl::PointCloud<PointLT>::Ptr densifyLabels(const pcl::PointCloud<PointLT>::Ptr label_cloud, const pcl::PointCloud<PointT>::Ptr ori_cloud);

model* TrainMultiSVM(std::string fea_path, std::string out_path, int l1, int l2, float CC, bool tacc_flag);
model* TrainBinarySVM(std::string fea_path, std::string out_path, int l1, int l2, float CC, bool tacc_flag);
//model* TrainBinarySVM(std::string fea_path, float CC, bool tacc_flag);

int main(int argc, char** argv)
{
    std::string in_path("/home/chi/JHUIT/scene/");
    std::string out_path("../../data_pool/semanticPR/binary_joint02/");
    
    pcl::console::parse_argument(argc, argv, "--p", in_path);
    pcl::console::parse_argument(argc, argv, "--o", out_path);
    boost::filesystem::create_directories(out_path);
    
    std::string svm_path("tmp_new/");
    std::string mesh_path("/home/chi/devel_mode/ObjRecRANSAC/data/mesh/");
    std::string shot_path("JHU_kmeans_dict/");
    std::string sift_path("JHU_sift_dict09/");
    std::string fpfh_path("JHU_fpfh_dict/");
    
/***************************************************************************************************************/
    float radius = 0.02;
    float down_ss = 0.005;
    float ratio = 0.1;
    float sigma = 0.9;

    pcl::console::parse_argument(argc, argv, "--rt", ratio);
    pcl::console::parse_argument(argc, argv, "--ss", down_ss);
    pcl::console::parse_argument(argc, argv, "--sigma", sigma);
    std::cerr << "Ratio: " << ratio << std::endl;
    std::cerr << "Downsample: " << down_ss << std::endl;
 
    float CC_M = 0.01, CC_B = 0.001;
    pcl::console::parse_argument(argc, argv, "--CCM", CC_M);
    pcl::console::parse_argument(argc, argv, "--CCB", CC_B);
    
    bool recompute_svm = false;
    if( pcl::console::find_switch(argc, argv, "-svm") == true )
        recompute_svm = true;
    
    bool view_flag = false;
    if( pcl::console::find_switch(argc, argv, "-v") == true )
        view_flag = true;
    
/***************************************************************************************************************/
    Hier_Pooler hie_producer(radius);
    hie_producer.LoadDict_L0(shot_path, "200", "200");
    hie_producer.setRatio(ratio);
/***************************************************************************************************************/
    std::vector< boost::shared_ptr<Pooler_L0> > sift_pooler_set(1+1);
    for( size_t i = 1 ; i < sift_pooler_set.size() ; i++ )
    {
        boost::shared_ptr<Pooler_L0> cur_pooler(new Pooler_L0(-1));
        sift_pooler_set[i] = cur_pooler;
    }
    sift_pooler_set[1]->LoadSeedsPool(sift_path+"dict_sift_L0_100.cvmat"); 
/***************************************************************************************************************/
    std::vector< boost::shared_ptr<Pooler_L0> > fpfh_pooler_set(1+1);
    for( size_t i = 1 ; i < fpfh_pooler_set.size() ; i++ )
    {
        boost::shared_ptr<Pooler_L0> cur_pooler(new Pooler_L0(-1));
        fpfh_pooler_set[i] = cur_pooler;
    }
    fpfh_pooler_set[1]->LoadSeedsPool(fpfh_path+"dict_fpfh_L0_100.cvmat");
/***************************************************************************************************************/
    std::vector< boost::shared_ptr<Pooler_L0> > lab_pooler_set(5+1);
    for( size_t i = 1 ; i < lab_pooler_set.size() ; i++ )
    {
        boost::shared_ptr<Pooler_L0> cur_pooler(new Pooler_L0);
        cur_pooler->setHSIPoolingParams(i);
        lab_pooler_set[i] = cur_pooler;
    }
/***************************************************************************************************************/
    
    std::vector<model*> binary_models(3);
    for( int ll = 0 ; ll <= 2 ; ll++ )
    {
        std::stringstream ss;
        ss << ll;
        if( exists_test(out_path+"binary_"+ss.str()+"_"+ss.str()+".model") == false || recompute_svm == true )
            binary_models[ll] = TrainBinarySVM(svm_path, out_path, ll, ll, CC_B, true);
        else
            binary_models[ll] = load_model((out_path+"binary_"+ss.str()+"_"+ss.str()+".model").c_str());
    }
    
//    std::vector<model*> multi_models(4);
//    for( int ll = 1 ; ll <= 3 ; ll++ )
//    {
//        std::stringstream ss;
//        ss << ll;
//        if( exists_test(out_path+"multi_"+ss.str()+"_"+ss.str()+".model") == false || recompute_svm == true )
//            multi_models[ll] = TrainMultiSVM(svm_path, out_path, ll, ll, CC_M, true);
//        else
//            multi_models[ll] = load_model((out_path+"multi_"+ss.str()+"_"+ss.str()+".model").c_str());
//    }

/***************************************************************************************************************/
    setObjID(model_name_map);
    std::vector<ModelT> model_set;
    std::vector<std::string> model_names = readMesh(mesh_path, model_set);
    int model_num = model_names.size();
/***************************************************************************************************************/
    
    pcl::visualization::PCLVisualizer::Ptr viewer;
    if( view_flag )
    {
        viewer = pcl::visualization::PCLVisualizer::Ptr (new pcl::visualization::PCLVisualizer ("3D Viewer"));
        viewer->initCameraParameters();
        viewer->addCoordinateSystem(0.1);
        viewer->setCameraPosition(0, 0, 0.1, 0, 0, 1, 0, -1, 0);
        viewer->setSize(1280, 960);
//        omp_set_num_threads(1);
    }

    std::vector< std::string > prefix_set(3);
    prefix_set[0] = "office";
    prefix_set[1] = "labpod";
    prefix_set[2] = "barrett";
    
    for( int tt = 0 ; tt <= 2 ; tt++ )
    {
        std::vector<int> obj_count(model_num+1, 0);
        std::vector< std::vector<PR_ELEM> > avg_model_pr(3);
        for( size_t k = 0 ; k < avg_model_pr.size() ; k++ )
        {
            avg_model_pr[k].resize(model_num+1);
            for( int i = 0 ; i < model_num+1 ; i++ )
            {
                avg_model_pr[k][i].precision = 0;
                avg_model_pr[k][i].recall = 0;
                avg_model_pr[k][i].f_score = 0;
                avg_model_pr[k][i].valid = true;
            }
        }
        int frame_count = 0;        
        std::vector< std::vector<sparseVec> > final_test(model_num+1);
        std::vector<std::string> scene_names;
        
        for( int i = 0 ; i < 10 ; i++ )
        {
            std::stringstream ss;
            ss << i;
            scene_names.push_back(std::string (prefix_set[tt] +"_"+ ss.str()));
        }

        for( std::vector<std::string>::iterator scene_it = scene_names.begin() ; scene_it < scene_names.end() ; scene_it++ )
        {
            std::string cur_path(in_path + *scene_it + "/");
            std::string gt_path(in_path + *scene_it + "/poses/");

            #pragma omp parallel for schedule(dynamic, 1)
            for( int i = 0 ; i <= 99 ; i++ )
            {
                std::stringstream ss;
                ss << i;

                std::string filename(cur_path + *scene_it + "_" + ss.str() + ".pcd");
                std::cerr << filename << std::endl;

                if( exists_test(filename) == false )//|| exists_test(filename_n) == false )
                {
                    pcl::console::print_warn("Failed to Read: %s\n", filename.c_str());
                    continue;
                }
                pcl::PointCloud<PointT>::Ptr full_cloud(new pcl::PointCloud<PointT>());
                pcl::io::loadPCDFile(filename, *full_cloud);
                std::vector<poseT> cur_gt = readGT(gt_path, ss.str());
                if( cur_gt.empty() == true ) continue;
                pcl::PointCloud<PointLT>::Ptr all_gt_cloud = genSeg(full_cloud, model_set, cur_gt, model_name_map);
                pcl::PointCloud<PointLT>::Ptr fore_gt_cloud(new pcl::PointCloud<PointLT>());
                pcl::copyPointCloud(*all_gt_cloud, *fore_gt_cloud);
                for( pcl::PointCloud<PointLT>::iterator it = fore_gt_cloud->begin() ; it < fore_gt_cloud->end(); it++ )
                    if( it->label > 0 )
                        it->label = 1;
                
                std::vector< std::vector<PR_ELEM> > cur_model_pr(avg_model_pr.size());
                
                spPooler triple_pooler;
                triple_pooler.init(full_cloud, hie_producer, radius, down_ss);
                triple_pooler.build_SP_LAB(lab_pooler_set, false);

                pcl::PointCloud<PointLT>::Ptr foreground_cloud(new pcl::PointCloud<PointLT>());
                for( int ll = 0 ; ll <= 2 ; ll++ )
                {
                    bool reset_flag = ll == 0 ? true : false;
                    triple_pooler.InputSemantics(binary_models[ll], ll, reset_flag, false);
                    foreground_cloud = triple_pooler.getSemanticLabels();
                    cur_model_pr[ll] = semanticPR(fore_gt_cloud, foreground_cloud, model_num);
                }
                
//                triple_pooler.InputSemantics(binary_models[0], 0, false, false);
//                pcl::PointCloud<PointLT>::Ptr foreground_cloud = triple_pooler.getSemanticLabels();
//                cur_model_pr[0] = semanticPR(fore_gt_cloud, foreground_cloud, model_num);
//                std::cerr << std::setprecision(4) << cur_model_pr[0][1].recall << "\t" << cur_model_pr[0][1].precision << "\t" << cur_model_pr[0][1].f_score << std::endl;
        
//                avg_model_pr[0][1].recall += cur_model_pr[0][1].recall;
//                avg_model_pr[0][1].precision += cur_model_pr[0][1].precision;
//                avg_model_pr[0][1].f_score += cur_model_pr[0][1].f_score;
                
                
//                if( view_flag )
//                {
//                    viewer->removeAllPointClouds();
//                    viewer->addPointCloud(full_cloud, "full");
//                    visualizeLabels(foreground_cloud, viewer, color_label);
//                    viewer->removeAllPointClouds();
//                }
                
//                triple_pooler.extractForeground();
//                triple_pooler.build_SP_FPFH(fpfh_pooler_set, radius, false);
//                triple_pooler.build_SP_SIFT(sift_pooler_set, hie_producer, sigma, false);
                
//                pcl::PointCloud<PointLT>::Ptr label_cloud(new pcl::PointCloud<PointLT>());
//                for( int ll = 1 ; ll <= 3 ; ll++ )
//                {
//                    bool reset_flag = ll == 1 ? true : false;
//                    triple_pooler.InputSemantics(multi_models[ll], ll, reset_flag, false);
//                    label_cloud = triple_pooler.getSemanticLabels();
//                    cur_model_pr[ll] = semanticPR(all_gt_cloud, label_cloud, model_num);
//                }
                triple_pooler.reset();
                #pragma omp critical
                {
                    for( int j = 1 ; j < model_num + 1 ; j++ )
                    {
                        if( cur_model_pr[0][j].valid )
                        {
//                            std::cerr << "Model " << j << std::endl;
                            for( size_t kk = 0 ; kk < avg_model_pr.size() ; kk++ )
                            {
                                avg_model_pr[kk][j].recall += cur_model_pr[kk][j].recall;
                                avg_model_pr[kk][j].precision += cur_model_pr[kk][j].precision;
                                avg_model_pr[kk][j].f_score += cur_model_pr[kk][j].f_score;
                                
//                                std::cerr << std::setprecision(4) << cur_model_pr[kk][j].recall << "\t" << cur_model_pr[kk][j].precision << "\t" << cur_model_pr[kk][j].f_score << std::endl;
                            }
                            obj_count[j]++;
                        }
                    }
		    frame_count++;
                }
//                if( view_flag )
//                {
//                    pcl::PointCloud<PointLT>::Ptr semantic_cloud = triple_pooler.getSemanticLabels();
//                    viewer->removeAllPointClouds();
//                    viewer->addPointCloud(full_cloud, "full");
//                    visualizeLabels(label_cloud, viewer, color_label);
//                    viewer->removeAllPointClouds();
//                }
                
                
            }
        }
        
        std::ofstream fp;
        fp.open((out_path + prefix_set[tt] + "_pr.txt").c_str());

//        std::cerr << std::setprecision(4) << avg_model_pr[0][1].recall/frame_count << "\t" << avg_model_pr[0][1].precision/frame_count << "\t" << avg_model_pr[0][1].f_score/frame_count << std::endl;
//        fp << std::setprecision(4) << avg_model_pr[0][1].recall/frame_count << "\t" << avg_model_pr[0][1].precision/frame_count << "\t" << avg_model_pr[0][1].f_score/frame_count << std::endl;
        
        for( int j = 1 ; j < model_num + 1 ; j++ )
        {
            std::cerr << obj_count[j] << std::endl;
            fp << obj_count[j] << std::endl;
            
            for( size_t kk = 0 ; kk < avg_model_pr.size() ; kk++ )
            {
                float cur_recall = avg_model_pr[kk][j].recall / obj_count[j];
                float cur_precision = avg_model_pr[kk][j].precision / obj_count[j];
                float cur_f = avg_model_pr[kk][j].f_score / obj_count[j];
                
                std::cerr << std::setprecision(4) << cur_recall << "\t" << cur_precision << "\t" << cur_f << std::endl;
                fp << std::setprecision(4) << cur_recall << "\t" << cur_precision << "\t" << cur_f << std::endl;
            }
        }
        fp.close();
    }
    
//    free_and_destroy_model(&multi_model);
    
    return 1;
} 

model* TrainMultiSVM(std::string fea_path, std::string out_path, int l1, int l2, float CC, bool tacc_flag)
{
    std::vector< std::pair<int, int> > piece_inds;
    piece_inds.push_back(std::pair<int, int> (0, 130000));
    std::vector<problem> train_prob_set;
    for( int i = 1 ; i <= 10  ; i++ )
    {
        std::stringstream ss;
        ss << i;
        for( int ll = l1 ; ll <= l2 ; ll++ )
        {
            std::stringstream mm;
            mm << ll;
        
            std::string train_name = fea_path + "train_"+ss.str()+"_L"+mm.str()+".smat";
            if( exists_test(train_name) == false )
                continue;
            
            std::cerr << "Reading: " << train_name << std::endl;
            std::vector<SparseDataOneClass> cur_data(1);
            int fea_dim = readSoluSparse_piecewise(train_name, cur_data[0].fea_vec, piece_inds);
            cur_data[0].label = i; 

            problem tmp;
            FormFeaSparseMat(cur_data, tmp, cur_data[0].fea_vec.size(), fea_dim);
            train_prob_set.push_back(tmp);
        }
    }
    problem train_prob;
    train_prob.l = 0;
    mergeProbs(train_prob_set, train_prob);
    
    parameter param;
    GenSVMParamter(param, CC);
    std::cerr<<std::endl<<"Starting Liblinear Training..."<<std::endl;
    
    std::stringstream gg1, gg2;
    gg1 << l1;
    gg2 << l2;
    model* cur_model = train(&train_prob, &param);
    save_model((out_path+"multi_"+gg1.str()+"_"+gg2.str()+".model").c_str(), cur_model);
    
    if ( tacc_flag == true ){
    	int corr_t[OBJ_INST_MAX] = {0}, count_t[OBJ_INST_MAX] = {0};
    	for(int j = 0 ; j < train_prob.l ; j++ )
    	{
            int true_label = floor(train_prob.y[j]+0.0001);
            count_t[true_label]++;
            double pred_tmp = predict(cur_model, train_prob.x[j]);
            int pred_label = floor(pred_tmp+0.0001);
            if( pred_label == true_label )
                corr_t[true_label]++;
    	}
    	int total_corr = 0;
    	for( int b1 = 1 ; b1 <= 10 ; b1++ )
    	{
            total_corr += corr_t[b1];
            std::cerr<<b1<<" "<<count_t[b1]<<" "<<corr_t[b1]/(count_t[b1]+0.0)<<std::endl;
    	}
    	std::cerr<<"Final Multi Training Accuracy: "<<(total_corr+0.0)/train_prob.l<<std::endl;
    }
    
    destroy_param(&param);
    free(train_prob.y);
    for( int i = 0 ; i < train_prob.l ; i++ )
        free(train_prob.x[i]);
    free(train_prob.x);
    return cur_model;
}


model* TrainBinarySVM(std::string fea_path, std::string out_path, int l1, int l2, float CC, bool tacc_flag)
{
    std::vector< std::pair<int, int> > piece_inds1;
    std::vector< std::pair<int, int> > piece_inds2;
    piece_inds2.push_back(std::pair<int, int> (0, 90000));
    std::vector<problem> train_prob_set;
    for( int i = 0 ; i <= 10  ; i++ )
    {
        std::stringstream ss;
        ss << i;
        for( int ll = l1 ; ll <= l2 ; ll++ )
        {
            std::stringstream mm;
            mm << ll;
        
            std::string train_name = fea_path + "train_"+ss.str()+"_L"+mm.str()+".smat";
            if( exists_test(train_name) == false )
                continue;
            
            std::cerr << "Reading: " << train_name << std::endl;
            std::vector<SparseDataOneClass> cur_data(1);
            
            int fea_dim = i == 0 ? readSoluSparse_piecewise(train_name, cur_data[0].fea_vec, piece_inds1) 
                                 : readSoluSparse_piecewise(train_name, cur_data[0].fea_vec, piece_inds2);
            cur_data[0].label = i == 0 ? 1 : 2; 

            problem tmp;
            FormFeaSparseMat(cur_data, tmp, cur_data[0].fea_vec.size(), fea_dim);
            train_prob_set.push_back(tmp);
        }
    }
    problem train_prob;
    train_prob.l = 0;
    mergeProbs(train_prob_set, train_prob);
    
    parameter param;
    GenSVMParamter(param, CC);
    std::cerr<<std::endl<<"Starting Liblinear Training..."<<std::endl;
    
    std::stringstream gg1, gg2;
    gg1 << l1;
    gg2 << l2;
    model* cur_model = train(&train_prob, &param);
    save_model((out_path+"binary_"+gg1.str()+"_"+gg2.str()+".model").c_str(), cur_model);
    
    if ( tacc_flag == true ){
    	int corr_t[OBJ_INST_MAX] = {0}, count_t[OBJ_INST_MAX] = {0};
    	for(int j = 0 ; j < train_prob.l ; j++ )
    	{
            int true_label = floor(train_prob.y[j]+0.0001 - 1);
            count_t[true_label]++;
            double pred_tmp = predict(cur_model, train_prob.x[j]);
            int pred_label = floor(pred_tmp+0.0001 - 1);
            if( pred_label == true_label )
                corr_t[true_label]++;
    	}
    	int total_corr = 0;
    	for( int b1 = 0 ; b1 <= 1 ; b1++ )
    	{
            total_corr += corr_t[b1];
            std::cerr<<b1<<" "<<count_t[b1]<<" "<<corr_t[b1]/(count_t[b1]+0.0)<<std::endl;
    	}
    	std::cerr<<"Final Binary Training Accuracy: "<<(total_corr+0.0)/train_prob.l<<std::endl;
    }
    
    destroy_param(&param);
    free(train_prob.y);
    for( int i = 0 ; i < train_prob.l ; i++ )
        free(train_prob.x[i]);
    free(train_prob.x);
    return cur_model;
}


std::vector<PR_ELEM> semanticPR(const pcl::PointCloud<PointLT>::Ptr gt_cloud, const pcl::PointCloud<PointLT>::Ptr label_cloud, int model_num)
{
    std::vector<PR_ELEM> model_pr(model_num + 1);
    
    if( label_cloud->empty() == true )
    {
        for(size_t i = 0 ; i < model_pr.size() ; i++ )
        {
            model_pr[i].precision = 0;
            model_pr[i].recall = 0;
            model_pr[i].f_score = 0;
            model_pr[i].valid = true;
        }
        return model_pr;
    }
    
    pcl::search::KdTree<PointLT> tree;
    tree.setInputCloud(label_cloud);
    
    std::vector<int> corr_count(model_num + 1, 0);
    std::vector<int> count_in_gt(model_num + 1, 0);
    std::vector<int> count_in_cloud(model_num + 1, 0);
    
    float T = 0.01;
    float sqrT = T*T;
    
    for( pcl::PointCloud<PointLT>::const_iterator it = gt_cloud->begin() ; it < gt_cloud->end() ; it++ )
    {
        if( pcl_isfinite(it->z) == false )
            continue;
        std::vector<int> indices (1);
	std::vector<float> sqr_distances (1);
        int nres = tree.nearestKSearch(*it, 1, indices, sqr_distances);
        if ( nres >= 1 && sqr_distances[0] <= sqrT )
        {
            uint32_t estimated_label = label_cloud->at(indices[0]).label;
            if( estimated_label == it->label )
                corr_count[it->label]++;
            count_in_cloud[estimated_label]++;
        }
        count_in_gt[it->label]++;
        
    }
    
    for( size_t i = 1 ; i < corr_count.size() ; i++ )
    {
        if( count_in_gt[i] <= 0 )
        {
            model_pr[i].valid = false;
            model_pr[i].recall = -1.0;
            model_pr[i].precision = -1.0;
            model_pr[i].f_score = -1.0;
        }
        else
        {
            float cur_recall = (corr_count[i] + 0.0) / count_in_gt[i];
            float cur_precision = count_in_cloud[i] == 0 ? 0.0 : (corr_count[i] + 0.0) / count_in_cloud[i];
            model_pr[i].valid = true;
            model_pr[i].recall = cur_recall;
            model_pr[i].precision = cur_precision;
            model_pr[i].f_score = cur_recall + cur_precision == 0 ? 0 : 2*(cur_precision*cur_recall)/(cur_recall+cur_precision);
        }
    }
    return model_pr;
}

/*model* TrainLinearSVM(std::string fea_path, int c1, int c2, float CC, bool tacc_flag)
{
    std::vector< std::pair<int, int> > piece_inds;
    std::vector<problem> train_prob_set;
    for( int i = c1 ; i <= c2  ; i++ )
    {
        std::stringstream ss;
        ss << i;
        std::string train_name = fea_path + "train_"+ss.str()+"_L0.smat";
        
        std::cerr << "Reading: " << train_name << std::endl;
        std::vector<SparseDataOneClass> cur_data(1);
        int fea_dim = readSoluSparse_piecewise(train_name, cur_data[0].fea_vec, piece_inds);
        cur_data[0].label = i; 

        problem tmp;
        FormFeaSparseMat(cur_data, tmp, cur_data[0].fea_vec.size(), fea_dim);
        train_prob_set.push_back(tmp);
    }
    problem train_prob;
    train_prob.l = 0;
    mergeProbs(train_prob_set, train_prob);
    
    parameter param;
    GenSVMParamter(param, CC);
    std::cerr<<std::endl<<"Starting Liblinear Training..."<<std::endl;
    
    model* cur_model = train(&train_prob, &param);
    save_model((fea_path+"svm_model.model").c_str(), cur_model);
    
    if ( tacc_flag == true ){
    	int corr_t[OBJ_INST_MAX] = {0}, count_t[OBJ_INST_MAX] = {0};
    	for(int j = 0 ; j < train_prob.l ; j++ )
    	{
            int true_label = floor(train_prob.y[j]+0.0001);
            count_t[true_label]++;
            double pred_tmp = predict(cur_model, train_prob.x[j]);
            int pred_label = floor(pred_tmp+0.0001);
            if( pred_label == true_label )
                corr_t[true_label]++;
    	}
    	int total_corr = 0;
    	for( int b1 = c1 ; b1 <= c2 ; b1++ )
    	{
            total_corr += corr_t[b1];
            std::cerr<<b1<<" "<<count_t[b1]<<" "<<corr_t[b1]/(count_t[b1]+0.0)<<std::endl;
    	}
    	std::cerr<<"Final Training Accuracy: "<<(total_corr+0.0)/train_prob.l<<std::endl;
    }
    
    destroy_param(&param);
    free(train_prob.y);
    for( int i = 0 ; i < train_prob.l ; i++ )
        free(train_prob.x[i]);
    free(train_prob.x);
    return cur_model;
}*/

/*for( int i = 0 ; i <= 9  ; i++ )
    {
        continue;
        std::stringstream ss;
        ss << i+1;
        
        if( exists_test(out_path + "train_"+ss.str()+"_L0.smat") == true )
            continue;
        
        ObjectSet train_objects, test_objects;
        readJHUInstWithImg(model_path, train_objects, test_objects, i, i, true);
        std::cerr << "Loading Completed... " << std::endl;
        
        int train_num = train_objects[0].size();
        std::cerr << "Train " << i << " --- " << train_num << std::endl;
        
        if( train_num > 0 )
        {
            std::vector< sparseVec> final_train;
            #pragma omp parallel for schedule(dynamic, 1)
            for( int j = 0 ; j < train_num ; j++ )
            {
                pcl::PointCloud<PointT>::Ptr mycloud = train_objects[0][j].cloud;
            	cv::Mat map2d = train_objects[0][j].map2d;
                cv::Mat img = train_objects[0][j].img;
                
                pcl::PointCloud<PointT>::Ptr full_cloud(new pcl::PointCloud<PointT>());
                full_cloud->resize(map2d.rows*map2d.cols);
                for(int r = 0, this_idx = 0 ; r < map2d.rows ; r++ ){
                    for(int c = 0 ; c < map2d.cols ; c++, this_idx++ )
                    {
                        int idx2 = map2d.at<int>(r, c);
                        if( idx2 >= 0 )
                        {
                            full_cloud->at(this_idx).x = mycloud->at(idx2).x;
                            full_cloud->at(this_idx).y = mycloud->at(idx2).y;
                            full_cloud->at(this_idx).z = mycloud->at(idx2).z;
                            full_cloud->at(this_idx).rgba = mycloud->at(idx2).rgba;
                        }
                        else
                        {
                            uint32_t rgba = img.at<uchar>(r, c*3+0) | img.at<uchar>(r, c*3+1) << 8 | img.at<uchar>(r, c*3+2) << 16;
                            full_cloud->at(this_idx).x = std::numeric_limits<float>::quiet_NaN();
                            full_cloud->at(this_idx).y = std::numeric_limits<float>::quiet_NaN();
                            full_cloud->at(this_idx).z = std::numeric_limits<float>::quiet_NaN();
                            full_cloud->at(this_idx).rgba = rgba;
                        }
                    }
                }
                full_cloud->height = map2d.rows;
                full_cloud->width = map2d.cols;
                full_cloud->is_dense = false;
                
                spPooler triple_pooler;
                triple_pooler.init(full_cloud, hie_producer, radius, down_ss);
                triple_pooler.build_SP_LAB(lab_pooler_set, false);
                triple_pooler.build_SP_FPFH(fpfh_pooler_set, radius, false);
                triple_pooler.build_SP_SIFT(sift_pooler_set, hie_producer, sigma, false);
                
                std::vector<cv::Mat> sp_fea_L2 = triple_pooler.sampleSPFea(2, box_num);
                std::vector<cv::Mat> sp_fea_L3 = triple_pooler.sampleSPFea(3, box_num);
                std::vector<cv::Mat> sp_fea_L4 = triple_pooler.sampleSPFea(4, box_num);
                
                std::vector<cv::Mat> sp_fea;
                sp_fea.insert(sp_fea.end(), sp_fea_L2.begin(), sp_fea_L2.end());
                sp_fea.insert(sp_fea.end(), sp_fea_L3.begin(), sp_fea_L3.end());
                sp_fea.insert(sp_fea.end(), sp_fea_L4.begin(), sp_fea_L4.end());
                for( std::vector<cv::Mat>::iterator it = sp_fea.begin(); it < sp_fea.end() ; it++ )
                {
                    if( fea_dim > 0 && it->cols != fea_dim )
                    {
                        std::cerr << "Error: fea_dim > 0 && cur_final.cols != fea_dim   " << fea_dim << " " << it->cols << std::endl;
                        exit(0);
                    }
                    else if( fea_dim < 0 )
                    {
                        #pragma omp critical
                        {
                            fea_dim = it->cols;
                            std::cerr << "Fea Dim: " << fea_dim << std::endl;
                        }
                    }	
                    std::vector< sparseVec> this_sparse;
                    sparseCvMat(*it, this_sparse);
                    #pragma omp critical
                    {
                        final_train.push_back(this_sparse[0]);
                    }
                }
                
            }
            
            saveCvMatSparse(out_path + "train_"+ss.str()+"_L0.smat", final_train, fea_dim);
            final_train.clear();
        }
        else
        {
            std::cerr << "JHU Model Data Reading Failed!" << std::endl;
            exit(0);
        }
        train_objects.clear();
    }*/
