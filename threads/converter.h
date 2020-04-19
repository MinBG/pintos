/*    include/threads/converter.h   */
#define convert (1<<14)
int i_to_f(int i);
int f_to_i(int f);
int f_to_i_r(int f);
int f_add_f(int f_1, int f_2);
int ff_sub_bf(int ff, int bf);
int f_add_i(int f, int i);
int i_add_f(int i, int f);
int i_sub_f(int i, int f);
int f_sub_i(int f, int i);

int f_mul_f(int f1, int f2);
int i_mul_f(int i, int f);
int f_mul_i(int f, int i);
int f_div_f(int f1, int f2);
int i_div_f(int i, int f);
int f_div_i(int f, int i);

int i_to_f(int i){
  return i*convert;
}
int f_to_i(int f){
  return f/convert;
}
int f_to_i_r(int f){
  if(f>=0){ return f/convert+convert/2/convert; }
  return f/convert-convert/2/convert;
}
int f_add_f(int f_1, int f_2){
  return f_1+f_2 ;
}
int ff_sub_bf(int ff, int bf){
  return ff-bf;
}
int f_add_i(int f, int i){
  return f+i*convert;
}
int i_add_f(int i, int f){
  return i*convert+f;
}
int i_sub_f(int i, int f){
  return i*convert-f;
}
int f_sub_i(int f, int i){
  return f-i*convert;
}

int f_mul_f(int f1, int f2){
  return ((int64_t)f1)*f2/convert;
}
int i_mul_f(int i, int f){
  return f*i;
}
int f_mul_i(int f, int i){
  return i*f;
}
int f_div_f(int f1, int f2){
  return ((int64_t)f1)*convert/f2;
}
int i_div_f(int i, int f){
  return i/f;
}
int f_div_i(int f, int i){
  return f/i;
}
