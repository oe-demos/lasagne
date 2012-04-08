// downloaded from: http://www.codeproject.com/Articles/201263/Part-6-Primitive-Restart-and-OpenGL-Interoperabili

//Code by Rob Farber
#include <iostream>
#include <fstream>
using namespace std;
 
#include <OpenCL/opencl.h>
#include <OpenCL/cl.h>
#include <OpenCL/cl_gl.h>
#include <GLUT/glut.h>
#include <OpenGL/CGLCurrent.h>

#include "GLSLShader.h"

#define WIDTH  1408
#define HEIGHT 1024

// Globals used in the program
const unsigned int      mesh_width = 128, mesh_height = 128;
const unsigned int RestartIndex = 0xffffffff;
 
cl_platform_id          platform;         
cl_device_id            device;
cl_context              context;
cl_command_queue        queue;
cl_program              program;
cl_kernel               kernel;
size_t                  kernelsize;
size_t                  global[] = {mesh_width, mesh_height};
char                    *pathname = NULL;
char                    *source = NULL; 
 
GLSLShader* shader;

// Globals associated with the position vbo
const unsigned int p_vbo_size = mesh_width*mesh_height*4*sizeof(float); 
GLuint  p_vbo;
cl_mem  p_vbocl;
 
// Globals associated with the color vbo
const unsigned int c_vbo_size = mesh_width*mesh_height*4*sizeof(unsigned char); 
GLuint  c_vbo;
cl_mem  c_vbocl;
 
// Globals associated with the indices for primitive restart
GLuint* qIndices=NULL;
int qIndices_size = 5*(mesh_height-1)*(mesh_width-1);
float   anim = 0.0;
//int drawMode=GL_TRIANGLE_FAN; // the default draw mode
int drawMode=GL_POINTS; // the default draw mode
const char* drawStr="fan";
const char* platformString="notset";
 
// Globals associated with the mouse controls
int mouse_old_x, mouse_old_y;
int mouse_buttons = 0;
float rotate_x = 0.0, rotate_y = 0.0;
float translate_z = -2.5;
 
// Forward references for the GLUT callbacks
void display();
void motion(int x, int y);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int x, int y);
void initgl(int argc, const char** argv);
 
// helper routine to set the window title
void setTitle()
{
  char title[256];
  sprintf(title, "GL Interop Wrapper: mode %s device %s",
         drawStr, platformString);  
  glutSetWindowTitle(title);
}
 
int main(int argc, const char **argv) 
{
  initgl(argc, argv);

  string vert = "void main() { gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; }";
  string frag = "uniform float time; void main() { gl_FragColor = vec4(sin(time),0,1,1); }";
  shader = new GLSLShader(vert, frag);
  
  clGetPlatformIDs(1, &platform, NULL);
  if(argc > 1) {
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
    platformString = "CPU";
  } else {
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    platformString = "GPU";
  }
  cout << "platform: " << platformString << std::endl;

    #if defined (__APPLE__) || defined(MACOSX)
        CGLContextObj kCGLContext = CGLGetCurrentContext();
        CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
        cl_context_properties props[] =
        {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties)kCGLShareGroup,
            0
        };
  // It is necessary to add the gl context to the properties or
  // nothing will display
#else
#ifdef _WIN32
  HGLRC glCtx = wglGetCurrentContext();
#else //!_WIN32
  GLXContext glCtx = glXGetCurrentContext();
#endif //!_WIN32
  
  cl_context_properties props[] = { CL_CONTEXT_PLATFORM, 
                  (cl_context_properties)platform,
#ifdef _WIN32
                  CL_WGL_HDC_KHR, (intptr_t) wglGetCurrentDC(),
#else //!_WIN32
                  CL_GLX_DISPLAY_KHR, (intptr_t) glXGetCurrentDisplay(),
#endif //!_WIN32
                  CL_GL_CONTEXT_KHR, (intptr_t) glCtx, 0};
  #endif
  // Create the context and the queue
  context = clCreateContext(props, 1, &device, NULL, NULL, NULL);
  
  queue = clCreateCommandQueue(context, device, 0, NULL); 
  
  // create position p_vbo
  glGenBuffers(1, &p_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, p_vbo);
  // initialize buffer object
  glBufferData(GL_ARRAY_BUFFER, p_vbo_size, 0, GL_DYNAMIC_DRAW);        
  // create OpenCL buffer from GL VBO
  p_vbocl = clCreateFromGLBuffer(context, CL_MEM_WRITE_ONLY, p_vbo, NULL);
 
  // create color c_vbo (very similar to the position vbo)
  glGenBuffers(1, &c_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, c_vbo);
  glBufferData(GL_ARRAY_BUFFER, c_vbo_size, 0, GL_DYNAMIC_DRAW);        
  c_vbocl = clCreateFromGLBuffer(context, CL_MEM_WRITE_ONLY, c_vbo, NULL);
  
  // For convenience use C++ to load the program source into memory
  ifstream file("sinewave.cl");
  string prog(istreambuf_iterator<char>(file), (istreambuf_iterator<char>()));
  file.close();
  const char* source = prog.c_str();
  const size_t kernelsize = prog.length()+1;
  program = clCreateProgramWithSource(context, 1, (const char**) &source,
                                 &kernelsize, NULL);
 
  // Build the program executable
  int err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  if (err != CL_SUCCESS) {
    size_t len;
    char buffer[2048];
    
    cerr << "Error: Failed to build program executable!" << endl;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                       sizeof(buffer), buffer, &len);
    cerr << buffer << endl;
    exit(1);
  }
  
  // Create the compute kernel in the program
  kernel = clCreateKernel(program, "sinewave", &err);
  if (!kernel || err != CL_SUCCESS) {
      cerr << "Error: Failed to create compute kernel!" << endl;

      if (err != CL_SUCCESS) {
          string error = "";
          switch(err) {
          case CL_INVALID_PROGRAM:
              error += "CL_INVALID_PROGRAM";
              break;
          case CL_INVALID_PROGRAM_EXECUTABLE:
              error += "CL_INVALID_PROGRAM_EXECUTABLE";
              break;
          case CL_INVALID_KERNEL_NAME:
              error += "CL_INVALID_KERNEL_NAME";
              break;
          case CL_INVALID_KERNEL_DEFINITION:
              error += "CL_INVALID_KERNEL_DEFINITION";
              break;
          case CL_INVALID_VALUE:
              error += "CL_INVALID_VALUE";
              break;
          case CL_OUT_OF_HOST_MEMORY:
              error += "CL_OUT_OF_HOST_MEMORY";
              break;
          default:
              error += "UNKNOWN";
          }
          cerr << "Error describtion: " << error << endl;
      }
      exit(1);
  }
  
  // Set the kernel arguments. Note argument 3 is set in display
  clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&p_vbocl);
  clSetKernelArg(kernel, 1, sizeof(unsigned int), &mesh_width);
  clSetKernelArg(kernel, 2, sizeof(unsigned int), &mesh_height);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), (void*)&c_vbocl);
  
  // Generate the indices for primitive restart
  // allocate and assign trianglefan indicies 
  qIndices = (GLuint *) malloc(qIndices_size*sizeof(GLint));
  int index=0;
  for(int i=1; i < mesh_height; i++) {
    for(int j=1; j < mesh_width; j++) {
      qIndices[index++] = (i)*mesh_width + j; 
      qIndices[index++] = (i)*mesh_width + j-1; 
      qIndices[index++] = (i-1)*mesh_width + j-1; 
      qIndices[index++] = (i-1)*mesh_width + j; 
      qIndices[index++] = RestartIndex;
    }
  }
  setTitle(); 
  glutMainLoop();
}
 
// setup the window and assign callbacks
void initgl(int argc, const char** argv) 
{
  glutInit(&argc, (char**)argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
  glutInitWindowPosition (glutGet(GLUT_SCREEN_WIDTH)/2 - WIDTH/2, 
                       glutGet(GLUT_SCREEN_HEIGHT)/2 - HEIGHT/2);
  glutInitWindowSize(WIDTH, HEIGHT);
  glutCreateWindow("");
  
  glutDisplayFunc(display);       // register GLUT callback functions
  glutKeyboardFunc(keyboard);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
 
  //glewInit();
  
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glDisable(GL_DEPTH_TEST);
  
  glViewport(0, 0, WIDTH, HEIGHT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(10000.0, (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1, 10.0);
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  return;
}
 
// This method is called everytime the screen is redisplayed. No
// optimization is performed as it recalculates the kernel every time.
void display() 
{
  anim += 0.001f;
  
  // map OpenGL buffer object for writing from OpenCL
  glFinish();
  clEnqueueAcquireGLObjects(queue, 1, &p_vbocl, 0,0,0);
  clEnqueueAcquireGLObjects(queue, 1, &c_vbocl, 0,0,0);
  
  // Set arg 3 and queue the kernel
  clSetKernelArg(kernel, 3, sizeof(float), &anim);
  clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global, NULL, 0, 0, 0);
  
  // queue unmap buffer object
  clEnqueueReleaseGLObjects(queue, 1, &c_vbocl, 0,0,0);
  clEnqueueReleaseGLObjects(queue, 1, &p_vbocl, 0,0,0);
  clFinish(queue);
  
  // clear graphics
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);     
 
  // Apply the image transforms
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, translate_z);
  glRotatef(rotate_x, 1.0, 0.0, 0.0);
  glRotatef(rotate_y, 0.0, 1.0, 0.0);
 
  //render from the p_vbo
  glBindBuffer(GL_ARRAY_BUFFER, p_vbo);
  glVertexPointer(4, GL_FLOAT, 0, 0);
  glEnableClientState(GL_VERTEX_ARRAY);
  
  // enable colors from the c_vbo
  glBindBuffer(GL_ARRAY_BUFFER, c_vbo);
  glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
  glEnableClientState(GL_COLOR_ARRAY);

  shader->Bind();
  shader->SetUniform("time", anim);
  
  // draw points, lines or triangles according to the user keyboard input
  switch(drawMode) {
  case GL_LINE_STRIP:
    for(int i=0 ; i < mesh_width*mesh_height; i+= mesh_width)
      glDrawArrays(GL_LINE_STRIP, i, mesh_width);
    break;
  default:
    glDrawArrays(GL_POINTS, 0, mesh_width * mesh_height);
    break;
  }
  
  // handle housekeeping and redisplay
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
  glutSwapBuffers();
  glutPostRedisplay();
}
 
// Keyboard events handler for GLUT
void keyboard(unsigned char key, int x, int y)
{
  switch(key) {
  case('q') :
  case(27) :
    exit(0);
  break;
  case 'd':
  case 'D':
    switch(drawMode) {
    case GL_POINTS: drawMode = GL_LINE_STRIP; drawStr = "line"; break;
    case GL_LINE_STRIP: drawMode = GL_TRIANGLE_FAN; drawStr = "fan"; break;
    default: drawMode=GL_POINTS; drawStr = "points"; break;
    }
  } 
  setTitle();
  glutPostRedisplay();
}
 
// Mouse event handler for GLUT
void mouse(int button, int state, int x, int y)
{
  if (state == GLUT_DOWN) {
    mouse_buttons |= 1<<button;
  } else if (state == GLUT_UP) {
    mouse_buttons = 0;
  }
  
  mouse_old_x = x;
  mouse_old_y = y;
  glutPostRedisplay();
}
 
// Motion event handler for GLUT
void motion(int x, int y)
{
  float dx, dy;
  dx = x - mouse_old_x;
  dy = y - mouse_old_y;
  
  if (mouse_buttons & 1) {
    rotate_x += dy * 0.2;
    rotate_y += dx * 0.2;
  } else if (mouse_buttons & 4) {
    translate_z += dy * 0.01;
  }
  
  mouse_old_x = x;
  mouse_old_y = y;
}

