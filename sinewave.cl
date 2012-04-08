__kernel void sinewave(__global float4* pos, unsigned int width, 
                     unsigned int height, float time, __global uchar4* color)
{
  unsigned int x = get_global_id(0);
  unsigned int y = get_global_id(1);
  
  // calculate uv coordinates
  float u = x / (float) width;
  float v = y / (float) height;
  u = u*2.0f - 1.0f;
  v = v*2.0f - 1.0f;
  
  // calculate simple sine wave pattern
  float freq = 4.0f;
  float w = sin(u*freq + time) * cos(v*freq + time) * 0.5f;
  
  // write output vertex
  pos[y*width+x] = (float4)(u, w, v, 1.0f);
  color[y*width+x] = (uchar4) ((uchar) 255.0f*0.5f*(1.0f+sin(w+x)), 
                               (uchar) 255.0f*0.5f*(1.0f+sin((float)x)*cos((float)y)),
                               (uchar) 255.0f *0.5f*(1.0f+sin(w+time/10.0f)), 0 );

  //color[y*width+x] = (uchar4) (255.0f *0.5f*(1.0f+sin(w+x)),255.0f *0.5f*(1.0f+sin(x)*cos(y)),255.0f *0.5f*(1.0f+sin(w+time/10.0f)), 0.0f );
}
