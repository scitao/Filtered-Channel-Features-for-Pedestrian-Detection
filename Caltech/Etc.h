

vector<string> get_all_files_names_within_folder(string folder)
{
	vector<string> names;
	char search_path[1000];
	sprintf_s(search_path, 1000, "%s*.*", folder.c_str());
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(search_path, &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			// read all (real) files in current folder
			// , delete '!' read other 2 default folder . and ..
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				names.push_back(fd.cFileName);
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	return names;
}
vector<string> get_all_folders_names_within_folder(string folder)
{
	vector<string> names;
	char search_path[1000];
	sprintf_s(search_path, 1000, "%s*.*", folder.c_str());
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(search_path, &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			// read all (real) files in current folder
			// , delete '!' read other 2 default folder . and ..
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				names.push_back(fd.cFileName);
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	return names;
}

vector<Mat> make_hogdescriptor_parsed(vector<float>& descriptorValues, Size winSize)
{

	vector<Mat> HOG_channel;
	for (int bin = 0; bin < bin_size; bin++)
	{
		Mat HOG_tmp(Size(winSize.width / cell_size, winSize.height / cell_size), CV_32FC1);

		for (int row = 0; row < winSize.height / cell_size; row++)
		{
			for (int col = 0; col < winSize.width / cell_size; col++)
				HOG_tmp.at<float>(row, col) = 0.0;
		}

		HOG_channel.push_back(HOG_tmp);
	}
		

	// prepare data structure: bin_size orientation / gradient strenghts for each cell
	int cells_in_x_dir = winSize.width / cell_size;
	int cells_in_y_dir = winSize.height / cell_size;
	int gradientBinSize = bin_size;
	
	
	Mat cellUpdateCounter(Size(winSize.width / cell_size, winSize.height / cell_size), CV_32SC1);
	for (int row = 0; row < winSize.height / cell_size; row++)
	{
		for (int col = 0; col < winSize.width / cell_size; col++)
			cellUpdateCounter.at<int>(row, col) = 0;
	}


	// nr of blocks = nr of cells - 1
	// since there is a new block on each cell (overlapping blocks!) but the last one
	int blocks_in_x_dir = cells_in_x_dir - 1;
	int blocks_in_y_dir = cells_in_y_dir - 1;

	// compute gradient strengths per cell
	int descriptorDataIdx = 0;
	int cellx = 0;
	int celly = 0;

	for (int blockx = 0; blockx<blocks_in_x_dir; blockx++)
	{
		for (int blocky = 0; blocky<blocks_in_y_dir; blocky++)
		{
			// 4 cells per block ...
			for (int cellNr = 0; cellNr<4; cellNr++)
			{
				// compute corresponding cell nr
				int cellx = blockx;
				int celly = blocky;
				if (cellNr == 1) celly++;
				if (cellNr == 2) cellx++;
				if (cellNr == 3)
				{
					cellx++;
					celly++;
				}

				for (int bin = 0; bin<gradientBinSize; bin++)
				{
					float gradientStrength = descriptorValues[descriptorDataIdx];
					descriptorDataIdx++;

					HOG_channel[bin].at<float>(celly, cellx) += gradientStrength;
					

				} // for (all bins)


				// note: overlapping blocks lead to multiple updates of this sum!
				// we therefore keep track how often a cell was updated,
				// to compute average gradient strengths
				cellUpdateCounter.at<int>(celly,cellx)++;
			
			} // for (all cells)


		} // for (all block x pos)
	} // for (all block y pos)


	// compute average gradient strengths
	for (int celly = 0; celly<cells_in_y_dir; celly++)
	{
		for (int cellx = 0; cellx<cells_in_x_dir; cellx++)
		{

			float NrUpdatesForThisCell = (float)cellUpdateCounter.at<int>(celly,cellx);

			// compute average gradient strenghts for each gradient bin direction
			for (int bin = 0; bin<gradientBinSize; bin++)
				HOG_channel[bin].at<float>(celly, cellx) /= NrUpdatesForThisCell;
		}
	}
	
	return HOG_channel;
}

vector<Mat> HOG_extract(Mat input_image, bool patch_size, int width, int height)
{
	Mat gray_image;
	cvtColor(input_image, gray_image, CV_BGR2GRAY);


	HOGDescriptor hog;
	hog.winSize = Size(width, height);
	hog.blockSize = Size(block_size, block_size);
	hog.blockStride = Size(block_stride, block_stride);
	hog.cellSize = Size(cell_size, cell_size);
	hog.nbins = bin_size;

	vector<float> hog_value;
	vector<Point> locations;
	hog.compute(gray_image, hog_value, Size(0, 0), Size(0, 0), locations);

	
	if (patch_size)
	{
		int elem;
		int x = padding;
		int y = padding;
		int y_index = y / cell_size;
		vector<float> resized_hog_value;

		for (int x_index = x / cell_size; x_index < x / cell_size + patch_col / cell_size - 1; x_index++)
		{
			elem = (bin_size * pow((block_size / cell_size), 2) * (height / cell_size - 1)) * x_index + (bin_size * pow((block_size / cell_size), 2)) * y_index;

			for (int c = (x_index - x / cell_size) * (patch_row / cell_size - 1) * pow((block_size / cell_size), 2) * bin_size; c < (x_index - x / cell_size + 1) * (patch_row / cell_size - 1) * pow((block_size / cell_size), 2) * bin_size; c++)
				resized_hog_value.push_back(hog_value.at(elem++));
		}

		vector<Mat> HOG_channel = make_hogdescriptor_parsed(resized_hog_value, Size(patch_col, patch_row));
		Mat gaussian_filter(Size(3, 1), CV_32FC1);
		gaussian_filter.at<float>(0, 0) = 0.25;
		gaussian_filter.at<float>(0, 1) = 0.5;
		gaussian_filter.at<float>(0, 2) = 0.25;

		vector<Mat> result;
		for (int bin = 0; bin < bin_size; bin++)
		{
			Mat HOG_tmp;
			filter2D(HOG_channel[bin], HOG_tmp, -1, gaussian_filter);
			result.push_back(HOG_tmp);
		}

		return result;
	}

	else
	{
		vector<Mat> HOG_channel = make_hogdescriptor_parsed(hog_value, Size(width, height));
		Mat gaussian_filter(Size(3, 1), CV_32FC1);
		gaussian_filter.at<float>(0, 0) = 0.25;
		gaussian_filter.at<float>(0, 1) = 0.5;
		gaussian_filter.at<float>(0, 2) = 0.25;

		vector<Mat> result;
		for (int bin = 0; bin < bin_size; bin++)
		{
			Mat HOG_tmp;
			filter2D(HOG_channel[bin], HOG_tmp, -1, gaussian_filter);
			result.push_back(HOG_tmp);
		}

		return result;

	}
		
}

vector<Mat> LUV_extract(Mat input_image, int width, int height)
{
	Mat luv_image;
	vector<Mat> LUV_channel;
	vector<Mat> result;

	cvtColor(input_image, luv_image, CV_BGR2Luv);
	split(luv_image, LUV_channel);

	for (int luv_index = 0; luv_index < luv_size; luv_index++)
	{
		Mat luv_tmp(Size(width / cell_size, height / cell_size), CV_32FC1);
		for (int row = 0; row < height / cell_size; row++)
		{
			for (int col = 0; col < width / cell_size; col++)
			{

				float value = 0;
				for (int row_conv = 0; row_conv < cell_size; row_conv++)
				{
					for (int col_conv = 0; col_conv < cell_size; col_conv++)
						value += (int)LUV_channel[luv_index].at<uchar>(row * cell_size + row_conv, col * cell_size + col_conv);
				}
				value /= (float)(cell_size * cell_size);
				luv_tmp.at<float>(row, col) = value;
			}
		}

		Mat gaussian_filter(Size(3, 1), CV_32FC1);
		gaussian_filter.at<float>(0, 0) = 0.25;
		gaussian_filter.at<float>(0, 1) = 0.5;
		gaussian_filter.at<float>(0, 2) = 0.25;

		Mat blurred;
		filter2D(luv_tmp, blurred, -1, gaussian_filter);
		result.push_back(blurred);
	}


	return result;
}

Mat GRADIENT_extract(Mat input_image, int width, int height)
{
	Mat gray_image;
	Mat gradient_mag;
	Mat gradient_mag_x;
	Mat gradient_mag_y;
	cvtColor(input_image, gray_image, CV_BGR2GRAY);


		Sobel(gray_image, gradient_mag_x, CV_8U, 1, 0);
		Sobel(gray_image, gradient_mag_y, CV_8U, 0, 1);
		gradient_mag = abs(gradient_mag_x) + abs(gradient_mag_y);

		Mat gradient_tmp(Size(width / cell_size, height / cell_size), CV_32FC1);
		for (int row = 0; row < height / cell_size; row++)
		{
			for (int col = 0; col < width / cell_size; col++)
			{
				float value = 0;
				for (int row_conv = 0; row_conv < cell_size; row_conv++)
				{
					for (int col_conv = 0; col_conv < cell_size; col_conv++)
						value += (int)gradient_mag.at<uchar>(row * cell_size + row_conv, col * cell_size + col_conv);
				}
				value /= (float)(cell_size * cell_size);
				gradient_tmp.at<float>(row, col) = value;
			}
		}

		Mat gaussian_filter(Size(3, 1), CV_32FC1);
		gaussian_filter.at<float>(0, 0) = 0.25;
		gaussian_filter.at<float>(0, 1) = 0.5;
		gaussian_filter.at<float>(0, 2) = 0.25;

		Mat blurred;
		filter2D(gradient_tmp, blurred, -1, gaussian_filter);

		return blurred;
}

vector<Mat> filter_generation(int type)
{
	vector<Mat> filter_list;

	if (type == ACF)	//ACF
	{
		Mat filter_mat(Size(1, 1), CV_32FC1);
		filter_mat.at<float>(0, 0) = 1;
		filter_list.push_back(filter_mat);
	}

	else if (type == CHECKBOARD)	//checkboard
	{
		for (int filter_row = 1; filter_row <= 4; filter_row++)
		{
			for (int filter_col = 1; filter_col <= 3; filter_col++)
			{
			
				//first type - all one
				Mat tmp(Size(filter_col, filter_row), CV_32FC1);
				for (int row = 0; row < filter_row; row++)
				{
					for (int col = 0; col < filter_col; col++)
						tmp.at<float>(row, col) = 1;
				}
				filter_list.push_back(tmp);
				//


				//second type - x direction gradient detector
				for (int x_direction = 0; x_direction < filter_col - 1; x_direction++)
				{
					Mat tmp(Size(filter_col, filter_row), CV_32FC1);
					for (int row = 0; row < filter_row; row++)
					{
						for (int col = 0; col < filter_col; col++)
						{
							if (col <= x_direction)
								tmp.at<float>(row, col) = 1;
							else
								tmp.at<float>(row, col) = -1;
							
						}
					}
					filter_list.push_back(tmp);
				}
				//


				//third type - y direction gradient detector
				for (int y_direction = 0; y_direction < filter_row - 1; y_direction++)
				{
					Mat tmp(Size(filter_col, filter_row), CV_32FC1);
					for (int row = 0; row < filter_row; row++)
					{
						for (int col = 0; col < filter_col; col++)
						{
							if (row <= y_direction)
								tmp.at<float>(row, col) = 1;
							else
								tmp.at<float>(row, col) = -1;
						}
					}
					filter_list.push_back(tmp);
				}
				//

				//forth type - checkboard pattern
				if (filter_row > 1 && filter_col > 1)
				{
					Mat tmp(Size(filter_col, filter_row), CV_32FC1);
					for (int row = 0; row < filter_row; row++)
					{
						for (int col = 0; col < filter_col; col++)
						{
							if (row % 2 == 0)
							{
								if (col % 2 == 0)
									tmp.at<float>(row, col) = 1;
								else
									tmp.at<float>(row, col) = -1;						
							}
							else
							{
								if (col % 2 == 0)
									tmp.at<float>(row, col) = -1;
								else
									tmp.at<float>(row, col) = 1;
							}
						}
					}

					filter_list.push_back(tmp);
				}
				//
			}
		}
		//
	}

	return filter_list;
}

vector<ground_truth> ground_truth_parsing(string folder, string sub_folder, string file, int& total_pedestrian)
{

	ifstream inf;
	stringstream ss;
	vector<string> folder_list;
	vector<string> sub_folder_list;
	vector<ground_truth> ground_truth_bb;
		
	string annotation_name = file;
	string from = ".jpg";
	string to = ".txt";
	size_t start_pos = annotation_name.find(from);
	annotation_name.replace(start_pos, from.length(), to);
			

	ss << ground_truth_dir << folder << "/" << sub_folder << "/" << annotation_name;
	inf.open(ss.str());

	ss.str("");
	ss.clear();

	string line;
	vector<string> line_save;
	while (getline(inf, line))
		line_save.push_back(line);

	for (int line_number = 1; line_number < line_save.size(); line_number++)
	{
		string id;
		int left, top, width, height;
		int v_left, v_top, v_width, v_height;
		int occ, ign, ang;
		ss << line_save.at(line_number);
		ss >> id >> left >> top >> width >> height >> occ >> v_left >> v_top >> v_width >> v_height >> ign >> ang;
		ss.str("");
		ss.clear();

		if (!id.compare("person") && !ign && height >= reasonable_height)
		{
			bool is_valid = false;
			if (occ)
			{
				int width_intersect = min(left + width, v_left + v_width) - max(left, v_left);
				int height_intersect = min(top + height, v_top + v_height) - max(height, v_height);
				int area_intersect = width_intersect * height_intersect;
				int area_union = width * height + v_width * v_height - area_intersect;
				double overlap_ratio = (double)(area_intersect) / (double)(area_union);
				overlap_ratio = 1 - overlap_ratio;
				if (width_intersect > 0 && height_intersect > 0 && overlap_ratio <= partial_occ)
					is_valid = true;
			}
			else
				is_valid = true;

			if (is_valid)
			{
				ss << image_dir << folder << "/" << sub_folder << "/" << file;


				Mat image;
				image = imread(ss.str());

				ss.str("");
				ss.clear();

				if (left < 0)
					left = 0;
				if (top < 0)
					top = 0;
				if (width < 0)
					width = 0;
				if (height < 0)
					height = 0;

				if (left + width > image.cols)
				{
					width = image.cols - left;

					while ((left + width - block_size) % block_stride != 0)
						width--;
				}
					

				if (top + height > image.rows)
				{
					height = image.rows - top;

					while ((top + height - block_size) % block_stride != 0)
						height--;
				}
					

				if (width > 0 && height > 0)
				{

					ground_truth gt_tmp;
					gt_tmp.x_max = left + width;
					gt_tmp.x_min = left;
					gt_tmp.y_max = top + height;
					gt_tmp.y_min = top;
					gt_tmp.is_valid = true;
					gt_tmp.is_detected = false;


					ground_truth_bb.push_back(gt_tmp);
					total_pedestrian++;

				}

				image.release();
			}
			else
			{
				ss << image_dir << folder << "/" << sub_folder << "/" << file;


				Mat image;
				image = imread(ss.str());

				ss.str("");
				ss.clear();

				if (left < 0)
					left = 0;
				if (top < 0)
					top = 0;
				if (width < 0)
					width = 0;
				if (height < 0)
					height = 0;

				if (left + width > image.cols)
				{
					width = image.cols - left;

					while ((left + width - block_size) % block_stride != 0)
						width--;
				}


				if (top + height > image.rows)
				{
					height = image.rows - top;

					while ((top + height - block_size) % block_stride != 0)
						height--;
				}


				if (width > 0 && height > 0)
				{

					ground_truth gt_tmp;
					gt_tmp.x_max = left + width;
					gt_tmp.x_min = left;
					gt_tmp.y_max = top + height;
					gt_tmp.y_min = top;
					gt_tmp.is_valid = false;
					gt_tmp.is_detected = false;


					ground_truth_bb.push_back(gt_tmp);

				}

				image.release();
			}
			

		}
		else
		{
			ss << image_dir << folder << "/" << sub_folder << "/" << file;


			Mat image;
			image = imread(ss.str());

			ss.str("");
			ss.clear();

			if (left < 0)
				left = 0;
			if (top < 0)
				top = 0;
			if (width < 0)
				width = 0;
			if (height < 0)
				height = 0;

			if (left + width > image.cols)
			{
				width = image.cols - left;

				while ((left + width - block_size) % block_stride != 0)
					width--;
			}


			if (top + height > image.rows)
			{
				height = image.rows - top;

				while ((top + height - block_size) % block_stride != 0)
					height--;
			}


			if (width > 0 && height > 0)
			{

				ground_truth gt_tmp;
				gt_tmp.x_max = left + width;
				gt_tmp.x_min = left;
				gt_tmp.y_max = top + height;
				gt_tmp.y_min = top;
				gt_tmp.is_valid = false;
				gt_tmp.is_detected = false;


				ground_truth_bb.push_back(gt_tmp);
			}

			image.release();
		}
	}

	

	return ground_truth_bb;
}

void NMS(vector<bounding_box> *bb_vector,  bounding_box bb)
{


	for (vector<bounding_box>::iterator it = bb_vector->begin(); it != bb_vector->end(); it++)
	{
		if (it->is_suppressed == false)
		{
			int width_intersect = min((it->rect.x + it->rect.width), (bb.rect.x + bb.rect.width)) - max(it->rect.x, bb.rect.x);
			int height_intersect = min((it->rect.y + it->rect.height), (bb.rect.y + bb.rect.height)) - max(it->rect.y, bb.rect.y);
			int area_intersect = width_intersect * height_intersect;
			int area_union = it->rect.width * it->rect.height + bb.rect.width * bb.rect.height - area_intersect;
			double overlap_ratio = (double)(area_intersect) / (double)(area_union);

			//if overlap ratio >0.5 or included area >0.5 -> delete!
			if (width_intersect > 0 && height_intersect > 0 && (overlap_ratio > 0.5 || area_intersect > 0.5*(it->rect.height*it->rect.width) || area_intersect > 0.5*(bb.rect.height*bb.rect.width)))
				it->is_suppressed = true;


		}

	}



}

inline float rand_float(float min, float max)
{
	return (float(rand()) / float(RAND_MAX))*(max - min) + min;
}