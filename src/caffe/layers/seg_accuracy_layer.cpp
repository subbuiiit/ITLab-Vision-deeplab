#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "caffe/layer.hpp"
#include "caffe/layers/seg_accuracy_layer.hpp"
#include "caffe/util/io.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void SegAccuracyLayer<Dtype>::LayerSetUp(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  confusion_matrix_.clear();
  confusion_matrix_.resize(bottom[0]->channels());
  has_ignore_label_ =
    this->layer_param_.accuracy_param().has_ignore_label();
  if (has_ignore_label_ == true) {
    ignore_label_ = this->layer_param_.accuracy_param().ignore_label();
  }
}

template <typename Dtype>
void SegAccuracyLayer<Dtype>::Reshape(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  CHECK_LE(1, bottom[0]->channels())
      << "top_k must be less than or equal to the number of channels (classes).";
  CHECK_EQ(bottom[0]->num(), bottom[1]->num())
    << "The data and label should have the same number.";
  CHECK_EQ(bottom[1]->channels(), 1)
    << "The label should have one channel.";
  CHECK_EQ(bottom[0]->height(), bottom[1]->height())
    << "The data should have the same height as label.";
  CHECK_EQ(bottom[0]->width(), bottom[1]->width())
    << "The data should have the same width as label.";
 
  top[0]->Reshape(1, 1, 1, 3);
}

template <typename Dtype>
void SegAccuracyLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  const Dtype* bottom_label = bottom[1]->cpu_data();
  int num = bottom[0]->num();
  int channels = bottom[0]->channels();
  int height = bottom[0]->height();
  int width = bottom[0]->width();

  int data_index, label_index;

  int top_k = 1;  // only support for top_k = 1

  // remove old predictions if exists
  confusion_matrix_.clear();  

  for (int i = 0; i < num; ++i) {
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        // Top-k accuracy
        std::vector<std::pair<Dtype, int> > bottom_data_vector;

        for (int c = 0; c < channels; ++c) {
          data_index = (c * height + h) * width + w;
          bottom_data_vector.push_back(std::make_pair(bottom_data[data_index], c));
        }

        std::partial_sort(
          bottom_data_vector.begin(), bottom_data_vector.begin() + top_k,
          bottom_data_vector.end(), std::greater<std::pair<Dtype, int> >());

        // check if true label is in top k predictions
        label_index = h * width + w;
        const int gt_label = static_cast<int>(bottom_label[label_index]);

        if ((has_ignore_label_ == true) && (gt_label == ignore_label_)) {
          continue;
        } else if ((0 <= gt_label) && (gt_label < channels)) {
          confusion_matrix_.accumulate(gt_label, bottom_data_vector[0].second);
        } else {
          LOG(FATAL) << "Unexpected gt label " << gt_label;
        }
      }
    }
    bottom_data  += bottom[0]->offset(1);
    bottom_label += bottom[1]->offset(1);
  }

  top[0]->mutable_cpu_data()[0] = static_cast<Dtype>(confusion_matrix_.accuracy());
  top[0]->mutable_cpu_data()[1] = static_cast<Dtype>(confusion_matrix_.avgRecall(false));
  top[0]->mutable_cpu_data()[2] = static_cast<Dtype>(confusion_matrix_.avgJaccard());
}

INSTANTIATE_CLASS(SegAccuracyLayer);
REGISTER_LAYER_CLASS(SegAccuracy);
}  // namespace caffe
