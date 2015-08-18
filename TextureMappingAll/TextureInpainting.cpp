#include "stdafx.h"
#include "TextureInpainting.h"
#include "Wang_Helper.h"
namespace WANG
{
	void RepairTexture(ObjInterface* _obj_data, int _r, int _patchsize)
	{
		printf("repair start!!\n");
		std::vector<int>& tex_id=_obj_data->tex_id;
		std::set<int>& invalid_texid=_obj_data->invalid_texid;
		std::string loadfilepath=_obj_data->loadfilepath;
		std::vector<std::string>& tex_filenames=_obj_data->tex_filenames;
		std::vector<Wml::Vector2f>& tex_points=_obj_data->tex_points;
		std::vector<Wml::Vector3i>& tex_faces=_obj_data->tex_faces;

		std::vector<std::vector<int>> faces_map;
		std::vector<std::vector<Wml::Vector2f>> parameterization;
		_obj_data->getInvalidfacesSets(faces_map, _r);
		//������Ҫ�����ͼ��
		std::map<int, cv::Mat3b> load_imgs;
		cv::Size s(200, 200);
		char savefilename[100];
		for (int i=1; i<faces_map.size(); ++i)
		{
			//PRINT(i);
			cv::Mat3f construct_new_img(s, cv::Vec3f(0, 0, 0));
			cv::Mat1b mask(s, 0);
			cv::Mat1b new_mask(s, 255);
			SimpleParameterization(_obj_data, faces_map[i], parameterization);
			//WANG_DEBUG_OUT;
			//���������Ƭ
			for (int j=0; j<faces_map[i].size(); ++j)
			{
				if(faces_map[i].size()<20) continue;
				int curf=faces_map[i][j];
				int cur_texid=tex_id[curf];
				if(invalid_texid.find(cur_texid)!=invalid_texid.end())
					fill_triangle(new_mask, parameterization[j]);
				else
				{
					//����Դͼ��
					if(load_imgs.find(cur_texid)==load_imgs.end()) load_imgs[cur_texid]=cv::imread(loadfilepath+tex_filenames[cur_texid]);
					std::vector<Wml::Vector2f> src_trangle(3);
					for(int k=0; k<3; ++k) src_trangle[k]=tex_points[tex_faces[curf][k]];
					copy_triangle(load_imgs[cur_texid], construct_new_img, mask, src_trangle, parameterization[j]);
				}
			}
			//WANG_DEBUG_OUT;
			//���ֵ ���ҽ�mask��ֵ��
			cv::Mat3b save_img(s);
			for (int r=0; r<construct_new_img.rows; ++r)
			{
				cv::Vec3f* fptr=construct_new_img[r];
				cv::Vec3b* save_img_ptr=save_img[r];
				uchar* mask_ptr=mask[r];
				for (int c=0; c<construct_new_img.cols; ++c)
				{
					if(mask_ptr[c]!=0)
					{
						fptr[c]=fptr[c]/mask_ptr[c];
						mask_ptr[c]=255;
					}
					save_img_ptr[c][0]=fptr[c][0];
					save_img_ptr[c][1]=fptr[c][1];
					save_img_ptr[c][2]=fptr[c][2];
				}
			}
			//WANG_DEBUG_OUT;
			//���������
			AddNewTexture(_obj_data, parameterization, faces_map[i], 200, 200);
			//�򵥲�ȫ
			simplecompletion(save_img, new_mask, 5);
			//WANG_DEBUG_OUT;
			//����������
			imwrite(loadfilepath+tex_filenames.back(), save_img);
			printf("save %s ok!!!\n", (loadfilepath+tex_filenames.back()).c_str());
		}
		//test
		//�����е��ڲ��ն�ȫ����.0�뵽һ��vector��
		/*std::vector<int> test_tmp;
		std::vector<std::vector<Point2D>> parameterization_sum;
		Simple_Parameterization(faces_map[0], parameterization);
		add_new_texture(parameterization, faces_map[0]);
		for(int i=1; i<faces_map.size(); ++i)
		{
		Simple_Parameterization(faces_map[i], parameterization);
		test_tmp.insert(test_tmp.end(), faces_map[i].begin(), faces_map[i].end());
		parameterization_sum.insert(parameterization_sum.end(), parameterization.begin(), parameterization.end());
		}
		add_new_texture(parameterization_sum, test_tmp);*/
		/*for (int i=0; i<faces_map.size(); ++i)
		{
		Simple_Parameterization(faces_map[i], parameterization);
		add_new_texture(parameterization, faces_map[i]);
		}*/
		//save_obj();
		//printf("repair end!!\n");
	}

	void SimpleParameterization(ObjInterface* _obj_data, std::vector<int>& _target_faces, std::vector<std::vector<Wml::Vector2f>>& _result)
	{
		//�����ڴ�
		std::vector<Wml::Vector3i>& faces=_obj_data->faces;
		std::vector<Wml::Vector3f>& points3d=_obj_data->points3d;
		_result.clear();
		_result.resize(_target_faces.size(), std::vector<Wml::Vector2f>(3));
		//����ÿ�������εķ�����
		Wml::Vector3f normals_sum(0.0, 0.0, 0.0);
		for (int i=0; i<_target_faces.size(); ++i)
		{
			Wml::Vector3i& curface=faces[_target_faces[i]];
			Wml::Vector3f v1, v2;
			v1=points3d[curface[1]]-points3d[curface[0]];
			v2=points3d[curface[2]]-points3d[curface[1]];
			normals_sum=normals_sum+v1.UnitCross(v2);
		}
		normals_sum.Normalize();
		//����������Ҫ��Ϊ�˽�z�����ɾ���� ����Z_baseҪ��Ӧnormals_sum
		Wml::Vector3f Z_base=normals_sum;
		Wml::Vector3f X_base(1, 1, -normals_sum.X()-normals_sum.Y());
		X_base.Normalize();
		Wml::Vector3f Y_base=Z_base.Cross(X_base);
		//�任����
		Wml::Matrix3f A(X_base, Y_base, Z_base, false);
		//����Ƭ��Ӧ�ĵ�ȫ�����б任
		float min_x, max_x, min_y, max_y;
		min_x=min_y=1e10;
		max_x=max_y=-1e10;
		for (int i=0; i<_target_faces.size(); ++i)
		{
			Wml::Vector3f p, Ap;
			Wml::Vector3i& curface=faces[_target_faces[i]];
			for (int j=0; j<3; ++j)
			{
				p=points3d[curface[j]];
				Ap=A*p;
				_result[i][j][0]=Ap.X();
				_result[i][j][1]=Ap.Y();
				min_x=MIN(min_x, Ap.X()); max_x=MAX(max_x, Ap.X());
				min_y=MIN(min_y, Ap.Y()); max_y=MAX(max_y, Ap.Y());
			}
		}
		//��һ��
		//min_x=min_x-0.05; max_x=max_x+0.05;
		//min_y=min_y-0.05; max_y=max_y+0.05;
		float x_len=max_x-min_x;
		float y_len=max_y-min_y;
		for (int i=0; i<_result.size(); ++i)
		{
			for (int j=0; j<3; ++j)
			{
				_result[i][j][0]=(_result[i][j][0]-min_x)/x_len;
				_result[i][j][1]=(_result[i][j][1]-min_y)/y_len;
			}
		}
	}

	void AddNewTexture(ObjInterface* _obj_data, std::vector<std::vector<Wml::Vector2f>>& _parameterization , std::vector<int>& _target_faces, int _width, int _height)
	{
		std::vector<int>& tex_id=_obj_data->tex_id;
		std::set<int>& invalid_texid=_obj_data->invalid_texid;
		std::vector<std::string>& tex_filenames=_obj_data->tex_filenames;
		std::vector<Wml::Vector2f>& tex_points=_obj_data->tex_points;
		std::vector<Wml::Vector3i>& tex_faces=_obj_data->tex_faces;
		std::vector<Wml::Vector4<int>>& tex_rois=_obj_data->tex_rois;
		int cur_vtsize=tex_points.size()-1;
		int new_texid=tex_filenames.size();
		char new_tex_filename[20];
		sprintf(new_tex_filename, "_texture%d.png", new_texid);
		//PRINT(new_tex_filename);
		//write mtl
		tex_filenames.push_back(new_tex_filename);
		tex_rois.push_back(Wml::Vector4<int>(0, 0, _width, _height)); 
		//add new vt
		//WANG_DEBUG_OUT;
		for (int i=0; i<_parameterization.size(); ++i)
		{
			int cur_face=_target_faces[i];
			//��������Ƭ������������Ͳ���ӵ��µ�������
			if(invalid_texid.find(tex_id[cur_face])==invalid_texid.end()) continue;
			tex_id[cur_face]=new_texid;
			for (int j=0; j<3; ++j)
			{
				++cur_vtsize;
				tex_points.push_back(_parameterization[i][j]);
				tex_faces[cur_face][j]=cur_vtsize;
			}
		}
		//WANG_DEBUG_OUT;
	}
}