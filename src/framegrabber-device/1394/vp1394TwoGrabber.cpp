/****************************************************************************
 *
 * $Id: vp1394TwoGrabber.cpp,v 1.35 2008-11-28 14:09:15 gfortier Exp $
 *
 * Copyright (C) 1998-2006 Inria. All rights reserved.
 *
 * This software was developed at:
 * IRISA/INRIA Rennes
 * Projet Lagadic
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * http://www.irisa.fr/lagadic
 *
 * This file is part of the ViSP toolkit
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE included in the packaging of this file.
 *
 * Licensees holding valid ViSP Professional Edition licenses may
 * use this file in accordance with the ViSP Commercial License
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Contact visp@irisa.fr if any conditions of this licensing are
 * not clear to you.
 *
 * Description:
 * Firewire cameras video capture.
 *
 * Authors:
 * Fabien Spindler
 *
 *****************************************************************************/


/*!
  \file vp1394TwoGrabber.cpp
  \brief member functions for firewire cameras
  \ingroup libdevice
*/
#include <iostream>

#include <visp/vpConfig.h>

/*
 * Interface with libdc1394 2.x
 */
#if defined(VISP_HAVE_DC1394_2)

#include <visp/vp1394TwoGrabber.h>
#include <visp/vpFrameGrabberException.h>
#include <visp/vpImageIo.h>
#include <visp/vpImageConvert.h>
#include <visp/vpTime.h>

const char * vp1394TwoGrabber::strVideoMode[DC1394_VIDEO_MODE_NUM]= {
  "MODE_160x120_YUV444",
  "MODE_320x240_YUV422",
  "MODE_640x480_YUV411",
  "MODE_640x480_YUV422",
  "MODE_640x480_RGB8",
  "MODE_640x480_MONO8",
  "MODE_640x480_MONO16",
  "MODE_800x600_YUV422",
  "MODE_800x600_RGB8",
  "MODE_800x600_MONO8",
  "MODE_1024x768_YUV422",
  "MODE_1024x768_RGB8",
  "MODE_1024x768_MONO8",
  "MODE_800x600_MONO16",
  "MODE_1024x768_MONO16",
  "MODE_1280x960_YUV422",
  "MODE_1280x960_RGB8",
  "MODE_1280x960_MONO8",
  "MODE_1600x1200_YUV422",
  "MODE_1600x1200_RGB8",
  "MODE_1600x1200_MONO8",
  "MODE_1280x960_MONO16",
  "MODE_1600x1200_MONO16",
  "MODE_EXIF",
  "MODE_FORMAT7_0",
  "MODE_FORMAT7_1",
  "MODE_FORMAT7_2",
  "MODE_FORMAT7_3",
  "MODE_FORMAT7_4",
  "MODE_FORMAT7_5",
  "MODE_FORMAT7_6",
  "MODE_FORMAT7_7"
};

const char * vp1394TwoGrabber::strFramerate[DC1394_FRAMERATE_NUM]= {
  "FRAMERATE_1_875",
  "FRAMERATE_3_75",
  "FRAMERATE_7_5",
  "FRAMERATE_15",
  "FRAMERATE_30",
  "FRAMERATE_60",
  "FRAMERATE_120",
  "FRAMERATE_240"
};

const char * vp1394TwoGrabber::strColorCoding[DC1394_COLOR_CODING_NUM]= {
  "COLOR_CODING_MONO8",
  "COLOR_CODING_YUV411",
  "COLOR_CODING_YUV422",
  "COLOR_CODING_YUV444",
  "COLOR_CODING_RGB8",
  "COLOR_CODING_MONO16",
  "COLOR_CODING_RGB16",
  "COLOR_CODING_MONO16S",
  "COLOR_CODING_RGB16S",
  "COLOR_CODING_RAW8",
  "COLOR_CODING_RAW16",
};



/*!
  Default constructor.

  By default:
  - the camera is the first found on the bus.
  - the ring buffer size is set to 4.

  Current camera settings can be changed using setCamera() to select the active
  camera on the bus and than setVideoMode() or setFramerate() to fix the active
  camera settings. The list of supported video modes and framerates is
  available using respectively getVideoModeSupported() and
  getFramerateSupported(). To change the ring buffer size use setRingBufferSize().

  \code
  vpImage<unsigned char> I;
  vp1394TwoGrabber g;
  g.setVideoMode(vp1394TwoGrabber::vpVIDEO_MODE_640x480_MONO8);
  g.setFramerate(vp1394TwoGrabber::vpFRAMERATE_15);
  while(1)
    g.acquire(I);
  \endcode

  \sa setCamera(), setVideoMode(), setFramerate()

*/
vp1394TwoGrabber::vp1394TwoGrabber( )
{
  // protected members
  width = height = 0;

  // private members
  num_cameras = 0;
  cameras = NULL;
  camera_id = 0;
  verbose = false;//true;
  camIsOpen = NULL;
  init = false;
  cameras = NULL;
#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
  d = NULL;
  list = NULL;
#endif
  num_buffers = 4; // ring buffer size

  initialize();
  //  open();
}

/*!

  Destructor.

  Close the firewire grabber.

  \sa close()

*/
vp1394TwoGrabber::~vp1394TwoGrabber()
{
  close();
}


/*!

  If multiples cameras are connected on the bus, select the camero to dial
  with.

  \param camera_id : A camera identifier. The value must be comprised
  between 0 (the first camera) and the number of cameras found on the
  bus and returned by getNumCameras() minus 1. If two cameras are
  connected on the bus, setting \e camera to one allows to communicate
  with the second one.

  \exception vpFrameGrabberException::settingError : If the required camera is
  not reachable.

  Here an example of single capture from the last camera found on the bus:
  \code
  unsigned int ncameras; // Number of cameras on the bus
  vpImage<unsigned char> I;
  vp1394TwoGrabber g;
  g.getNumCameras(ncameras);
  g.setCamera(ncameras-1); // To dial with the last camera on the bus
  while(1)
    g.acquire(I);// I contains the frame captured by the last camera on the bus
  \endcode

  Here an example of multi camera capture:
  \code
  unsigned int ncameras; // Number of cameras on the bus
  vp1394TwoGrabber g;
  g.getNumCameras(ncameras);
  vpImage<unsigned char> *I = new vpImage<unsigned char> [ncameras];

  // If the first camera supports vpVIDEO_MODE_640x480_YUV422 video mode
  g.setCamera(0);
  g.setVideoMode(vp1394TwoGrabber::vpVIDEO_MODE_640x480_YUV422);

  // If the second camera support 30 fps acquisition
  g.setCamera(1);
  g.setFramerate(vp1394TwoGrabber::vpFRAMERATE_30);

  while(1) {
    for (unsigned int camera=0; camera < ncameras; camera ++) {
      g.setCamera(camera);
      g.acquire(I[camera]);
    }
  }
  delete [] I;
  \endcode


  \sa setFormat(), setVideoMode(), setFramerate(), getNumCameras()

*/
void
vp1394TwoGrabber::setCamera(unsigned int camera_id)
{
  if (camera_id >= num_cameras) {
    close();
    vpERROR_TRACE("The required camera %u is not present", camera_id);
    vpERROR_TRACE("Only %u camera on the bus.", num_cameras);
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "The required camera is not present") );
  }

  this->camera_id =  camera_id;

  // create a pointer to the working camera
  camera = cameras[camera_id];
}

/*!

  Get the active camera identifier on the bus.

  \param camera_id : The active camera identifier. The value is
  comprised between 0 (the first camera) and the number of cameras
  found on the bus returned by getNumCameras() minus 1.

  \exception vpFrameGrabberException::initializationError : If no
  camera is found.

  \sa setCamera(), getNumCameras()

*/
void
vp1394TwoGrabber::getCamera(unsigned int &camera_id)
{
  if (num_cameras) {
    camera_id = this->camera_id;
  }
  else {
    close();
    vpERROR_TRACE("No cameras found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No cameras found") );
  }
}

/*!

  Return the number of cameras connected on the bus.

  \param ncameras : The number of cameras found on the bus.


*/
void
vp1394TwoGrabber::getNumCameras(unsigned int &ncameras)
{
  if (! num_cameras) {
    vpCTRACE << "No camera found..."<< std::endl;
    ncameras = 0;
  }

  ncameras = num_cameras;
}

/*!

  Set the camera video capture mode. Image size is than updated with respect to
  the new video capture mode.

  The iso transmission (setTransmission()) and the dma capture (see
  setCapture()) are first stopped. Then, the camera video capture mode is
  set. Finaly, the dma capture and the iso transmission are re-started.

  \param videomode : The camera video capture mode. The current camera mode is
  given by getVideoMode(). The camera supported modes are given by
  getVideoModeSupported().

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't set
  the video mode.

  \sa getVideoMode(), getVideoModeSupported(), setCamera()

*/
void
vp1394TwoGrabber::setVideoMode(vp1394TwoVideoModeType videomode)
{
  open();
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }
  if (!isVideoModeSupported(videomode)){
    vpERROR_TRACE("Video mode not supported by camera %d",camera_id);
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Video mode not supported") );
    return ;
  }
  // Stop dma capture if started
  setTransmission(DC1394_OFF);
  setCapture(DC1394_OFF);

  if (dc1394_video_set_mode(camera, (dc1394video_mode_t) videomode) != DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't set video mode");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't set video mode") );
  }

  setCapture(DC1394_ON);
  setTransmission(DC1394_ON);

  // Updates image size from new video mode
  if (dc1394_get_image_size_from_video_mode(camera,
      (dc1394video_mode_t) videomode,
      &this->width, &this->height)
      != DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't set video mode");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't get image size") );
  }

}

/*!

  Query the actual capture video mode of the active camera. All
  the active camera supported modes are given by getVideoModeSupported().

  \param videomode : The camera capture video mode.

  \exception vpFrameGrabberException::initializationError : If the
  required camera is not present.

  \exception vpFrameGrabberException::settingError : If we can't get
  the camera actual video mode.

  \sa setVideoMode(), getVideoModeSupported(), setCamera()

*/
void
vp1394TwoGrabber::getVideoMode(vp1394TwoVideoModeType & videomode)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  dc1394video_mode_t _videomode;
  if (dc1394_video_get_mode(camera, &_videomode) != DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't get current video mode");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't get current video mode") );
  }
  videomode = (vp1394TwoVideoModeType) _videomode;

}



/*!

  Query the available active camera video modes.


  \param videomodes : The list of supported camera video modes.

  \return The number of supported camera modes, 0 if an error occurs.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't get
  video modes.

  \sa setVideoMode(), getVideoMode(), getCamera()
*/
int
vp1394TwoGrabber::getVideoModeSupported(vpList<vp1394TwoVideoModeType> & videomodes)
{
  // Refresh the list of supported modes
  videomodes.kill();

  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }
  dc1394video_modes_t _videomodes;

  // get video modes:
  if (dc1394_video_get_supported_modes(camera, &_videomodes)!=DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't get video modes");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't get video modes") );
  }

  // parse the video modes to add in the list
  for (unsigned i=0; i < _videomodes.num; i++) {
    vp1394TwoVideoModeType _mode = (vp1394TwoVideoModeType) _videomodes.modes[i];
    videomodes.addRight( _mode );
  }

  // return the number of available video modes
  return _videomodes.num;
}
/*!
  Check for the active camera video mode.

  \param videomode : The video mode to check for.

  \return true if the active camera supports the desired video mode.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't get
  video modes.

  \sa setVideoMode(), getVideoMode(), getCamera()
 */
bool
vp1394TwoGrabber::isVideoModeSupported(vp1394TwoVideoModeType videomode)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }
  dc1394video_modes_t _videomodes;

  // get video modes:
  if (dc1394_video_get_supported_modes(camera, &_videomodes)!=DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't get video modes");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't get video modes") );
  }

  // parse the video modes to check with the desired
  for (unsigned i=0; i < _videomodes.num; i++) {
    if ((vp1394TwoVideoModeType) _videomodes.modes[i] == videomode)
      return true;
  }
  return false;
}

/*!

  Indicates if the video mode is format 7.

  \param videomode : The video mode to check for.
  
  \return true : If the video mode is scalable (Format 7).
  \return false : If the video mode is not Format 7 like.

  \sa setVideoMode(), getVideoModeSupported(), setCamera()

*/
bool
vp1394TwoGrabber::isVideoModeFormat7(vp1394TwoVideoModeType  videomode)
{

  if (dc1394_is_video_mode_scalable((dc1394video_mode_t) videomode))
    return true;

  return false;
}

/*!

  Indicates if the active camera is grabbing color or grey images.

  We consider color images if the color coding is either YUV (411, 422, 444) or
  RGB (8, 16, 16S).  We consider grey images if the color coding is MONO (8,
  16, 16S) or RAW (8, 16). vp1394TwoColorCodingType gives the supported color
  codings.

  \return true : If color images are acquired.
  \return false : If grey images are acquired.

  \sa getColorCoding(), setCamera()

*/
bool
vp1394TwoGrabber::isColor()
{
  vp1394TwoColorCodingType coding;
  getColorCoding(coding);

  switch (coding) {
    case vpCOLOR_CODING_MONO8:
    case vpCOLOR_CODING_MONO16:
    case vpCOLOR_CODING_MONO16S:
    case vpCOLOR_CODING_RAW8:
    case vpCOLOR_CODING_RAW16:
      return false;
    case vpCOLOR_CODING_YUV411:
    case vpCOLOR_CODING_YUV422:
    case vpCOLOR_CODING_YUV444:
    case vpCOLOR_CODING_RGB8:
    case vpCOLOR_CODING_RGB16:
    case vpCOLOR_CODING_RGB16S:
      return true;
  }
  return false;
}

/*!

  Set the active camera framerate for non scalable video modes.

  The iso transmission (setTransmission()) and the dma capture (see
  setCapture()) are first stopped. Then, the camera framerate capture mode is
  set. Finaly, the dma capture and the iso transmission are re-started.

  If the current video mode is scalable (Format 7), this function is without
  effect.

  \param fps : The camera framerate. The current framerate of the camera is
  given by getFramerate(). The camera supported framerates are given by
  getFramerateSupported().

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't set
  the framerate.

  \sa getFramerate(), getFramerateSupported() , setCamera()

*/
void
vp1394TwoGrabber::setFramerate(vp1394TwoFramerateType fps)
{
  open();
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  vp1394TwoVideoModeType cur_videomode;
  getVideoMode(cur_videomode);
  if (isVideoModeFormat7(cur_videomode))
    return;

  if (!isFramerateSupported(cur_videomode,fps)){
    vpERROR_TRACE("Framerate not supported by camera %d",camera_id);
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Framerate not supported") );
    return ;
  }

  // Stop dma capture if started
  setTransmission(DC1394_OFF);
  setCapture(DC1394_OFF);

  if (dc1394_video_set_framerate(camera, (dc1394framerate_t) fps) != DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't set framerate");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't set framerate") );
  }

  setCapture(DC1394_ON);
  setTransmission(DC1394_ON);
}

/*!

  Query the actual camera framerate of the active camera. The camera supported
  framerates are given by getFramerateSupported().

  \param fps : The camera capture framerate.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't get
  the framerate.

  \sa setFramerate(), getFramerateSupported(), setCamera()

*/
void
vp1394TwoGrabber::getFramerate(vp1394TwoFramerateType & fps)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }
  dc1394framerate_t _fps;
  if (dc1394_video_get_framerate(camera, &_fps) != DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't get current framerate");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't get current framerate") );
  }
  fps = (vp1394TwoFramerateType) _fps;

}

/*!

  Query the available framerates for the given camera video mode (see
  file dc1394/control.h). No framerate is associated to the following
  camera modes :

  - vp1394TwoGrabber::vpVIDEO_MODE_EXIF (format 6),
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_0 (format 7):
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_1 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_2 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_3 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_4 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_5 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_6 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_7 (format 7)

  \param mode : Camera video mode.

  \param fps : The list of supported camera framerates for the given camera
  video mode.

  \return The number of supported framerates, 0 if no framerate is available.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't get
  the supported framerates.

  \sa setFramerate(), getFramerate(), setCamera()
*/
int
vp1394TwoGrabber::getFramerateSupported(vp1394TwoVideoModeType mode,
                                        vpList<vp1394TwoFramerateType> & fps)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  // Refresh the list of supported framerates
  fps.kill();

  switch (mode) {
      // Framerate not available for:
      //  - vpVIDEO_MODE_EXIF ie Format_6
      //  - vpVIDEO_MODE_FORMAT7... ie the Format_7
    case vpVIDEO_MODE_EXIF:
    case vpVIDEO_MODE_FORMAT7_0:
    case vpVIDEO_MODE_FORMAT7_1:
    case vpVIDEO_MODE_FORMAT7_2:
    case vpVIDEO_MODE_FORMAT7_3:
    case vpVIDEO_MODE_FORMAT7_4:
    case vpVIDEO_MODE_FORMAT7_5:
    case vpVIDEO_MODE_FORMAT7_6:
    case vpVIDEO_MODE_FORMAT7_7:
      return 0;
      break;
    default:
    {
      dc1394framerates_t _fps;
      if (dc1394_video_get_supported_framerates(camera,
          (dc1394video_mode_t)mode,
          &_fps) != DC1394_SUCCESS) {
        close();
        vpERROR_TRACE("Could not query supported frametates for mode %d\n",
                      mode);
        throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                       "Could not query supported framerates") );
      }
      if (_fps.num == 0)
        return 0;

      for (unsigned int i = 0; i < _fps.num; i ++)
        fps.addRight((vp1394TwoFramerateType)_fps.framerates[i]);

      return _fps.num;
    }
    break;
  }
}
/*!

  Check if the desired framerate is supported for the given camera video mode
  (see file dc1394/control.h). No framerate is associated to the following
  camera modes :

  - vp1394TwoGrabber::vpVIDEO_MODE_EXIF (format 6),
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_0 (format 7):
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_1 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_2 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_3 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_4 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_5 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_6 (format 7)
  - vp1394TwoGrabber::vpVIDEO_MODE_FORMAT7_7 (format 7)

  \param mode : Camera video mode.

  \param fps : The desired camera framerates for the given camera
  video mode.

  \return true if the desired framerate is supported by the active camera.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't get
  the supported framerates.

\exception vpFrameGrabberException::settingError : If the framerate is not
  supported.
  \sa setFramerate(), getFramerate(), setCamera()
 */
bool
vp1394TwoGrabber::isFramerateSupported(vp1394TwoVideoModeType mode,
                                       vp1394TwoFramerateType fps)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  switch (mode) {
      // Framerate not available for:
      //  - vpVIDEO_MODE_EXIF ie Format_6
      //  - vpVIDEO_MODE_FORMAT7... ie the Format_7
    case vpVIDEO_MODE_EXIF:
    case vpVIDEO_MODE_FORMAT7_0:
    case vpVIDEO_MODE_FORMAT7_1:
    case vpVIDEO_MODE_FORMAT7_2:
    case vpVIDEO_MODE_FORMAT7_3:
    case vpVIDEO_MODE_FORMAT7_4:
    case vpVIDEO_MODE_FORMAT7_5:
    case vpVIDEO_MODE_FORMAT7_6:
    case vpVIDEO_MODE_FORMAT7_7:
      return 0;
      break;
    default:
    {
      dc1394framerates_t _fps;
      if (dc1394_video_get_supported_framerates(camera,
          (dc1394video_mode_t)mode,
          &_fps) != DC1394_SUCCESS) {
        close();
        vpERROR_TRACE("Could not query supported frametates for mode %d\n",
                      mode);
        throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                       "Could not query supported framerates") );
      }
      if (_fps.num == 0)
        return 0;

      for (unsigned int i = 0; i < _fps.num; i ++){
        if (fps==(vp1394TwoFramerateType)_fps.framerates[i]){
          return true;
        }
      }
      return false;
    }
    break;
  }
}

/*!

  Set the active camera Format 7 color coding.

  The iso transmission (setTransmission()) and the dma capture (see
  setCapture()) are first stopped. Then, the active camera Format 7 is
  set. Finaly, the dma capture and the iso transmission are re-started.

  \warning Setting color coding for non format 7 video mode will be
  without effect.

  \param coding : The camera color coding for Format 7 video mode. The
  current color coding of the camera is given by getColorCoding(). The
  camera supported color codings are given by
  getColorCodingSupported().

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't set
  the color coding for Format 7 video mode.

  \sa getColorCoding(), getColorCodingSupported() , setCamera()

*/
void
vp1394TwoGrabber::setColorCoding(vp1394TwoColorCodingType coding)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  dc1394video_mode_t _videomode;
  if (dc1394_video_get_mode(camera, &_videomode) != DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't get current video mode");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't get current video mode") );
  }

  if (!isColorCodingSupported((vp1394TwoVideoModeType)_videomode,coding)){
    vpERROR_TRACE("Color coding not supported by camera %d",camera_id);
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Color coding not supported") );
    return ;
  }

  // Format 7 video mode
  if (dc1394_is_video_mode_scalable(_videomode)) {
    setTransmission(DC1394_OFF);
    setCapture(DC1394_OFF);

    if (dc1394_format7_set_color_coding(camera, _videomode,
                                        (dc1394color_coding_t) coding)
        != DC1394_SUCCESS) {

      close();
      vpERROR_TRACE("Can't set color coding");
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Can't set color coding") );
    }

    setCapture(DC1394_ON);
    setTransmission(DC1394_ON);
  }
}

/*!

  Query the actual color coding of the active camera. The camera supported
  color codings are given by getColorCodingSupported().

  \param coding : The camera capture color coding.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't get
  the actual color coding. Occurs if current video mode is
  vp1394TwoGrabber::vpVIDEO_MODE_EXIF (format 6).

  \sa setColorCoding(), getColorCodingSupported(), setCamera()

*/
void
vp1394TwoGrabber::getColorCoding(vp1394TwoColorCodingType & coding)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }
  dc1394video_mode_t _videomode;
  if (dc1394_video_get_mode(camera, &_videomode) != DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't get current video mode");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't get current video mode") );
  }

  dc1394color_coding_t _coding;
  if (dc1394_is_video_mode_scalable(_videomode)) {
    // Format 7 video mode
    if (dc1394_format7_get_color_coding(camera, _videomode, &_coding)
        != DC1394_SUCCESS) {

      close();
      vpERROR_TRACE("Can't get current color coding");
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Can't query current color coding") );
    }
  }
  else if (dc1394_is_video_mode_still_image((dc1394video_mode_t)_videomode)) {
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "No color coding for format 6 video mode"));
  }
  else {
    // Not Format 7 and not Format 6 video modes
    if (dc1394_get_color_coding_from_video_mode(camera,
        (dc1394video_mode_t)_videomode,
        &_coding) != DC1394_SUCCESS) {
      close();
      vpERROR_TRACE("Could not query supported color coding for mode %d\n",
                    _videomode);
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Can't query current color coding"));
    }
  }
  coding = (vp1394TwoColorCodingType) _coding;
}

/*!

  Query the available color codings for the given camera video mode (see
  file dc1394/control.h).

  \param mode : Camera video mode.

  \param codings : The list of supported color codings for the given camera
  video mode.

  \return The number of supported color codings, 0 if no color codings
  is available.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't get
  the color codingss.

  \sa setColorCoding(), getColorCoding(), setCamera()
*/
int
vp1394TwoGrabber::getColorCodingSupported(vp1394TwoVideoModeType mode,
    vpList<vp1394TwoColorCodingType> & codings)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  // Refresh the list of supported framerates
  codings.kill();

  if (dc1394_is_video_mode_scalable((dc1394video_mode_t)mode)) {
    // Format 7 video mode
    dc1394color_codings_t _codings;
    if (dc1394_format7_get_color_codings(camera,
                                         (dc1394video_mode_t)mode,
                                         &_codings) != DC1394_SUCCESS) {
      close();
      vpERROR_TRACE("Could not query supported color codings for mode %d\n",
                    mode);
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Could not query supported color codings") );
    }
    if (_codings.num == 0)
      return 0;

    for (unsigned int i = 0; i < _codings.num; i ++)
      codings.addRight((vp1394TwoColorCodingType)_codings.codings[i]);

    return _codings.num;
  }
  else if (dc1394_is_video_mode_still_image((dc1394video_mode_t)mode)) {
    // Format 6 video mode
    return 0;
  }
  else  {
    // Not Format 7 and not Format 6 video modes
    dc1394color_coding_t _coding;
    if (dc1394_get_color_coding_from_video_mode(camera,
        (dc1394video_mode_t)mode,
        &_coding) != DC1394_SUCCESS) {
      close();
      vpERROR_TRACE("Could not query supported color coding for mode %d\n",
                    mode);
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Could not query supported color coding") );
    }
    codings.addRight((vp1394TwoColorCodingType)_coding);
    return 1;
  }
}
/*!

  Check if the color coding is supported for the given camera video mode (see
  file dc1394/control.h).

  \param mode : Camera video mode.

  \param coding : Desired color coding for the given camera
  video mode.

  \return true if the color coding is supported.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't get
  the color codingss.
  \exception vpFrameGrabberException::settingError : If the color coding is
  not supported.
  \sa setColorCoding(), getColorCoding(), setCamera()
 */
bool
vp1394TwoGrabber::isColorCodingSupported(vp1394TwoVideoModeType mode,
    vp1394TwoColorCodingType coding)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }


  if (dc1394_is_video_mode_scalable((dc1394video_mode_t)mode)) {
    // Format 7 video mode
    dc1394color_codings_t _codings;
    if (dc1394_format7_get_color_codings(camera,
                                         (dc1394video_mode_t)mode,
                                         &_codings) != DC1394_SUCCESS) {
      close();
      vpERROR_TRACE("Could not query supported color codings for mode %d\n",
                    mode);
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Could not query supported color codings") );
    }
    if (_codings.num == 0)
      return 0;

    for (unsigned int i = 0; i < _codings.num; i ++){
      if (coding==(vp1394TwoColorCodingType)_codings.codings[i])
        return true;
    }
    return false;
  }
  else if (dc1394_is_video_mode_still_image((dc1394video_mode_t)mode)) {
    // Format 6 video mode
    return false;
  }
  else  {
    // Not Format 7 and not Format 6 video modes
    dc1394color_coding_t _coding;
    if (dc1394_get_color_coding_from_video_mode(camera,
        (dc1394video_mode_t)mode,
        &_coding) != DC1394_SUCCESS) {
      close();
      vpERROR_TRACE("Could not query supported color coding for mode %d\n",
                    mode);
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Could not query supported color coding") );
      return false;
    }
    if (coding==(vp1394TwoColorCodingType)_coding)
      return true;

    return false;
  }
}


/*!

  Set the grabbed region of interest ie roi position and size for format 7
  video mode.

  The iso transmission (setTransmission()) and the dma capture (see
  setCapture()) are first stopped. Then, the format 7 roi is
  set. Finaly, the dma capture and the iso transmission are re-started.

  \warning Setting format 7 roi takes only effect if video mode is
  format 7 like.

  \param left : Position of the upper left roi corner.

  \param top : Position of the upper left roi corner.

  \param width : Roi width. If width is set to 0, uses the maximum
  allowed image width.

  \param height : Roi height. If width is set to 0, uses the maximum
  allowed image height.


  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't set
  roi.

  \sa isVideoModeFormat7()
*/
void
vp1394TwoGrabber::setFormat7ROI(unsigned int left, unsigned int top,
                                unsigned int width, unsigned int height)
{
  open();
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  dc1394video_mode_t _videomode;
  if (dc1394_video_get_mode(camera, &_videomode) != DC1394_SUCCESS) {

    close();
    vpERROR_TRACE("Can't get current video mode");
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Can't get current video mode") );
  }
  if (dc1394_is_video_mode_scalable(_videomode)) {
    // Stop dma capture if started
    setTransmission(DC1394_OFF);
    setCapture(DC1394_OFF);
    // Format 7 video mode
    unsigned int max_width, max_height;
    if (dc1394_format7_get_max_image_size(camera, _videomode,
                                          &max_width, &max_height)
        != DC1394_SUCCESS) {

      close();
      vpERROR_TRACE("Can't get format7 max image size");
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Can't get format7 max image size") );
    }
#if 0
    vpTRACE("left: %d top: %d width: %d height: %d", left, top,
            width == 0 ? DC1394_USE_MAX_AVAIL: width,
            height == 0 ? DC1394_USE_MAX_AVAIL : height);
    vpTRACE("max_width: %d max_height: %d", max_width, max_height);
#endif

    if (left > max_width) {
      vpERROR_TRACE("Can't set format7 ROI");
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Can't set format7 ROI") );
    }
    if (top > max_height) {
      vpERROR_TRACE("Can't set format7 ROI");
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Can't set format7 ROI") );
    }

    int roi_width;
    int roi_height;

    if (width != 0) {
      // Check if roi width is acceptable (ie roi is contained in the image)
      if (width > (max_width - left))
        width = (max_width - left);
      roi_width = width;
    }
    else {
      roi_width = DC1394_USE_MAX_AVAIL;
    }

    if (height != 0) {
      // Check if roi height is acceptable (ie roi is contained in the image)
      if (height > (max_height - top))
        height = (max_height - top);
      roi_height = height;
    }
    else {
      roi_height = DC1394_USE_MAX_AVAIL;
    }


    if (dc1394_format7_set_roi(camera, _videomode,
                               (dc1394color_coding_t) DC1394_QUERY_FROM_CAMERA, // color_coding
                               DC1394_USE_MAX_AVAIL/*DC1394_QUERY_FROM_CAMERA*/, // bytes_per_packet
                               left, // left
                               top, // top
                               roi_width,
                               roi_height)
        != DC1394_SUCCESS) {
      close();
      vpERROR_TRACE("Can't set format7 roi");
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Can't get current video mode") );
    }
    // Update the image size
    if (dc1394_format7_get_image_size(camera, _videomode,
                                      &this->width,
                                      &this->height)
        != DC1394_SUCCESS) {
      close();
      vpERROR_TRACE("Can't get format7 image size");
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Can't get format7 image size") );
    }

    setCapture(DC1394_ON);
    setTransmission(DC1394_ON);
  }
}
/*!

  Open ohci and asign handle to it and get the camera nodes and
  describe them as we find them.

  \exception initializationError : If a raw1394 handle can't be aquired,
  or if no camera is found.

  \sa close()
 */
void
vp1394TwoGrabber::initialize()
{
  if (init == false){
    // Find cameras
#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
    if (d != NULL)
      dc1394_free (d);

    d = dc1394_new ();
    if (dc1394_camera_enumerate (d, &list) != DC1394_SUCCESS) {
      dc1394_camera_free_list (list);
      close();
      vpERROR_TRACE("Failed to enumerate cameras\n");
      throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                     "Failed to enumerate cameras") );
    }

    if (list->num == 0) {
      dc1394_camera_free_list (list);
      close();
      vpERROR_TRACE("No cameras found");
      throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                     "No cameras found") );
    }

    if (cameras != NULL)
      delete [] cameras;

    cameras = new dc1394camera_t * [list->num];

    num_cameras = 0;
        

    for (unsigned int i=0; i < list->num; i ++) {
      cameras[i] = dc1394_camera_new (d, list->ids[i].guid);
      if (!cameras[i]) {
        vpTRACE ("Failed to initialize camera with guid \"%ld\"\n",
                 list->ids[i].guid);
        continue;
      }
      // Update the number of working cameras
      num_cameras ++;
    }

    // Reset the bus to make firewire working if the program was not properly
    // stopped by a CTRL-C. We reset here only the bus attached to the first
    // camera
    //dc1394_reset_bus(cameras[0]);

    if (list != NULL)
      dc1394_camera_free_list (list);
    list = NULL;

#elif defined VISP_HAVE_DC1394_2_FIND_CAMERAS // old API <= libdc1394-2.0.0-rc7
    if (cameras != NULL)
      free(cameras);
    cameras = NULL;
    int err = dc1394_find_cameras(&cameras, &num_cameras);

    if (err!=DC1394_SUCCESS && err != DC1394_NO_CAMERA) {
      close();
      vpERROR_TRACE("Unable to look for cameras\n\n"
                    "Please check \n"
                    "  - if the kernel modules `ieee1394',`raw1394' and `ohci1394' are loaded \n"
                    "  - if you have read/write access to /dev/raw1394\n\n");
      throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                     "Unable to look for cameras") );

    }
#endif

    if (num_cameras == 0) {
      close();
      vpERROR_TRACE("No cameras found");
      throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                     "No cameras found") );
    }

    if (camera_id >= num_cameras) {
      // Bad camera id
      close();
      vpERROR_TRACE("Bad camera id: %u", camera_id);
      vpERROR_TRACE("Only %u camera on the bus.", num_cameras);
      throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                     "Bad camera id") );
    }

    if (verbose) {
      std::cout << "------ Bus information ------" << std::endl;
      std::cout << "Number of camera(s) on the bus : " << num_cameras <<std::endl;
      std::cout << "-----------------------------" << std::endl;
    }

    if (camIsOpen != NULL) delete [] camIsOpen;
    camIsOpen = new bool [num_cameras];
    for (unsigned int i = 0;i<num_cameras;i++){
      camIsOpen[i]=false;
    }
    init = true;
  }
}
/*!

  Start the iso transmission and the dma capture of the current camera.

  \exception initializationError : If a raw1394 handle can't be aquired,
  or if no camera is found.

  \sa close()
*/
void
vp1394TwoGrabber::open()
{
  if (init == false) initialize();
  if (camIsOpen[camera_id] == false){
    dc1394switch_t status = DC1394_OFF;

    //#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
    dc1394_video_get_transmission(cameras[camera_id], &status);
    if (status != DC1394_OFF){
      //#endif
      if (dc1394_video_set_transmission(cameras[camera_id],DC1394_OFF)!=DC1394_SUCCESS)
        vpTRACE("Could not stop ISO transmission");
      else {
        vpTime::wait(500);
        if (dc1394_video_get_transmission(cameras[camera_id], &status)!=DC1394_SUCCESS)
          vpTRACE("Could get ISO status");
        else {
          if (status==DC1394_ON) {
            vpTRACE("ISO transmission refuses to stop");
          }
#ifdef VISP_HAVE_DC1394_2_FIND_CAMERAS // old API <= libdc1394-2.0.0-rc7
          // No yet in the new API
          cameras[camera_id]->is_iso_on=status;
#endif
        }
        //#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
      }
      //#endif
    }
    setCamera(camera_id);
    setIsoSpeed(DC1394_ISO_SPEED_400);
    setCapture(DC1394_ON);
    setTransmission(DC1394_ON);
    camIsOpen[camera_id] = true;
  }
}
/*!

  Close the firewire grabber.

  Stops the capture and the iso transmission of the active cameras and than
  releases all the cameras.

*/
void
vp1394TwoGrabber::close()
{
  if (init){
    if (num_cameras) {
      for (unsigned int i = 0; i < num_cameras;i++) {
        if (camIsOpen[i]) {
          camera = cameras[i];
          setTransmission(DC1394_OFF);
          setCapture(DC1394_OFF);
        }
#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
        dc1394_camera_free(cameras[i]);
#elif defined VISP_HAVE_DC1394_2_FIND_CAMERAS // old API <= libdc1394-2.0.0-rc7
        dc1394_free_camera(cameras[i]);
#endif
      }
    }
    if (camIsOpen != NULL) delete [] camIsOpen;

#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
    if (list != NULL){
      dc1394_camera_free_list (list);
      list = NULL;
    }
    if (d != NULL) {
      dc1394_free (d);
      d = NULL;
    }
    if (cameras != NULL) {
      delete [] cameras;
      cameras = NULL;
    }

#elif defined VISP_HAVE_DC1394_2_FIND_CAMERAS // old API <= libdc1394-2.0.0-rc7
    if (cameras != NULL) {
      free(cameras);
      cameras = NULL;
    }
#endif

    camIsOpen = NULL;
    num_cameras = 0;

    init = false;
  }
}

/*!

  Set the ring buffer size used for the capture.
  To know the current ring buffer size see getRingBufferSize().

  \param size : Ring buffer size.

  \exception vpFrameGrabberException::settingError : If ring buffer size is not valid.

  \sa getRingBufferSize()
*/
void
vp1394TwoGrabber::setRingBufferSize(unsigned int size)
{
  if (size < 1)  {
    close();
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Could not set ring buffer size") );
  }

  if (size != num_buffers) {
    // We need to change the ring buffer size
    num_buffers = size;
    if(camIsOpen[camera_id]){  
      setCapture(DC1394_OFF);
      setCapture(DC1394_ON);
    }  
  }
}

/*!

  Get the current ring buffer size used for the capture. To change the
  ring buffer size see setRingBufferSize().

  \return Current ring buffer size.

  \sa setRingBufferSize()
*/
unsigned int
vp1394TwoGrabber::getRingBufferSize()
{
  return num_buffers;
}

/*!

  Setup camera capture using dma. A ring buffer is used for the
  capture. It's size can be set using setRingBufferSize().

  \param _switch : Camera capture switch:
  - DC1394_ON to start dma capture,
  - DC1394_OFF to stop camera capture.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't set
  dma capture.

  \sa setRingBufferSize(), setVideoMode(), setFramerate()
*/
void
vp1394TwoGrabber::setCapture(dc1394switch_t _switch)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  if (_switch == DC1394_ON) {
    //if (dc1394_capture_setup(camera, num_buffers) != DC1394_SUCCESS) {
    // To be compatible with libdc1394 svn 382 version
    if (dc1394_capture_setup(camera, num_buffers,
                             DC1394_CAPTURE_FLAGS_DEFAULT) != DC1394_SUCCESS) {
      vpERROR_TRACE("Unable to setup camera capture-\n"
                    "make sure that the video mode and framerate are "
                    "supported by your camera.\n");
      close();
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Could not setup dma capture") );
    }
  }
  else { // _switch == DC1394_OFF
    dc1394error_t code = dc1394_capture_stop(camera);

    if (code != DC1394_SUCCESS && code != DC1394_CAPTURE_IS_NOT_SET) {
      vpERROR_TRACE("Unable to stop camera capture\n");
      close();
      throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                     "Could not setup dma capture") );
    }
  }
}


/*!

  Setup camera transmission.

  \param _switch : Transmission switch:
  - DC1394_ON to start iso transmission,
  - DC1394_OFF to stop iso transmission.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::settingError : If we can't set
  the video mode.
*/
void
vp1394TwoGrabber::setTransmission(dc1394switch_t _switch)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  dc1394switch_t status = DC1394_OFF;

  if (dc1394_video_get_transmission(camera, &status)!=DC1394_SUCCESS) {
    vpERROR_TRACE("Unable to get transmision status");
    close();
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Could not setup dma capture") );
  }

  //    if (status!=_switch){
  // Start dma capture if halted
  if (dc1394_video_set_transmission(camera, _switch) != DC1394_SUCCESS) {
    vpERROR_TRACE("Unable to setup camera capture-\n"
                  "make sure that the video mode and framerate are "
                  "supported by your camera.\n");
    close();
    throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                   "Could not setup dma capture") );
  }

  if (_switch == DC1394_ON) {
    status = DC1394_OFF;

    int i = 0;
    while ( status == DC1394_OFF && i++ < 5 ) {
      usleep(50000);
      if (dc1394_video_get_transmission(camera, &status)!=DC1394_SUCCESS) {
        vpERROR_TRACE("Unable to get transmision status");
        close();
        throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                       "Could not setup dma capture") );
      }
    }
  }
  //    }
}

/*!
  Speeds over 400Mbps are only available in "B" mode.

  \param speed : Iso data transmission speed.
*/
void
vp1394TwoGrabber::setIsoSpeed(dc1394speed_t speed)
{
  dc1394_video_set_iso_speed(camera, speed);
}

/*!
  Exist only for compatibility with other grabbing devices.

  Call acquire(vpImage<unsigned char> &I)

  \param I : Image data structure (8 bits image)

  \sa acquire(vpImage<unsigned char> &I)

*/
void
vp1394TwoGrabber::open(vpImage<unsigned char> &I)
{
  open();
  acquire(I);
}

/*!
  Exist only for compatibility with other grabbing devices.

  Call acquire(vpImage<vpRGBa> &I)

  \param I : Image data structure (RGBa format)

  \sa acquire(vpImage<vpRGBa> &I)

*/
void
vp1394TwoGrabber::open(vpImage<vpRGBa> &I)
{
  open();
  acquire(I);
}

/*!

  Get an image from the active camera frame buffer. This buffer neads to be
  released by enqueue().

  \return Pointer to the libdc1394-2.x image data structure.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \code
  vp1394TwoGrabber g;
  dc1394video_frame_t *frame;
  g.setVideoMode(vp1394TwoGrabber::vpVIDEO_MODE_640x480_MONO8);
  g.setFramerate(vp1394TwoGrabber::vpFRAMERATE_15);
  while(1) {
    frame = g.dequeue();
    // Current image is now in frame structure
    g.enqueue(frame);
  }

  \endcode

  \sa enqueue()
*/
dc1394video_frame_t *
vp1394TwoGrabber::dequeue()
{

  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  dc1394video_frame_t *frame = NULL;

  if (dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_WAIT, &frame)
      !=DC1394_SUCCESS) {
    vpERROR_TRACE ("Error: Failed to capture from camera %d\n", camera_id);
  }


  return frame;
}

/*!
  Release the frame buffer used by the active camera.

  \param frame : Pointer to the libdc1394-2.x image data structure.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \sa dequeue()
*/
void
vp1394TwoGrabber::enqueue(dc1394video_frame_t *frame)
{

  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  if (frame)
    dc1394_capture_enqueue(camera, frame);

}


/*!
  Acquire a grey level image from the active camera.

  \param I : Image data structure (8 bits image).

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus or if can't get camera settings.

  \exception vpFrameGrabberException::otherError : If format
  conversion to return a 8 bits image is not implemented.

  \sa setCamera(), setVideoMode(), setFramerate(), dequeue(), enqueue()
*/
void
vp1394TwoGrabber::acquire(vpImage<unsigned char> &I)
{
  uint32_t timestamp;
  uint32_t id;
  
  this->acquire(I, timestamp, id);
}

/*!
  Acquire a grey level image from the active camera.

  \param I : Image data structure (8 bits image).

  \param timestamp : The unix time in microseconds
  at which the frame was captured in the ring buffer.

  \param id : The frame position in the ring buffer.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus or if can't get camera settings.

  \exception vpFrameGrabberException::otherError : If format
  conversion to return a 8 bits image is not implemented.

  \sa setCamera(), setVideoMode(), setFramerate(), dequeue(), enqueue()
*/
void
vp1394TwoGrabber::acquire(vpImage<unsigned char> &I, 
			  uint32_t &timestamp,
			  uint32_t &id)
{
  open();
  dc1394video_frame_t *frame;

  frame = dequeue();

  // Timeval data structure providing the unix time
  // [microseconds] at which the frame was captured in the ring buffer.
  timestamp = frame->timestamp;
  id = frame->id;

  this->width  = frame->size[0];
  this->height = frame->size[1];
  unsigned int size = this->width * this->height;

  if ((I.getWidth() != this->width)||(I.getHeight() != this->height))
    I.resize(this->height, this->width);

  vp1394TwoColorCodingType color_coding;
  getColorCoding(color_coding);
  switch (color_coding) {
      //  switch(frame->color_coding) {
    case DC1394_COLOR_CODING_MONO8:
      memcpy(I.bitmap, (unsigned char *) frame->image,
             size*sizeof(unsigned char));
      break;
    case DC1394_COLOR_CODING_MONO16:
      vpImageConvert::MONO16ToGrey( (unsigned char *) frame->image,
                                    I.bitmap, size);
      break;

    case DC1394_COLOR_CODING_YUV411:
      vpImageConvert::YUV411ToGrey( (unsigned char *) frame->image,
                                    I.bitmap, size);
      break;

    case DC1394_COLOR_CODING_YUV422:
      vpImageConvert::YUV422ToGrey( (unsigned char *) frame->image,
                                    I.bitmap, size);
      break;

    case DC1394_COLOR_CODING_YUV444:
      vpImageConvert::YUV444ToGrey( (unsigned char *) frame->image,
                                    I.bitmap, size);
      break;

    case DC1394_COLOR_CODING_RGB8:
      vpImageConvert::RGBToGrey((unsigned char *) frame->image, I.bitmap, size);
      break;


    default:
      close();
      vpERROR_TRACE("Format conversion not implemented. Acquisition failed.");
      throw (vpFrameGrabberException(vpFrameGrabberException::otherError,
                                     "Format conversion not implemented. "
                                     "Acquisition failed.") );
      break;
  };
  enqueue(frame);
}



/*!
  Acquire a color image from the active camera.

  \param I : Image data structure (32 bits RGBa image).

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::otherError : If format
  conversion to return a RGBa bits image is not implemented.

  \sa setCamera(), setVideoMode(), setFramerate(), dequeue(), enqueue()
*/
void
vp1394TwoGrabber::acquire(vpImage<vpRGBa> &I)
{
  uint32_t timestamp;
  uint32_t id;
  
  this->acquire(I, timestamp, id);
}

/*!
  Acquire a color image from the active camera.

  \param I : Image data structure (32 bits RGBa image).

  \param timestamp : The unix time in microseconds
  at which the frame was captured in the ring buffer.

  \param id : The frame position in the ring buffer.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \exception vpFrameGrabberException::otherError : If format
  conversion to return a RGBa bits image is not implemented.

  \sa setCamera(), setVideoMode(), setFramerate(), dequeue(), enqueue()
*/
void
vp1394TwoGrabber::acquire(vpImage<vpRGBa> &I, 
			  uint32_t &timestamp,
			  uint32_t &id)
{
  open();
  dc1394video_frame_t *frame;

  frame = dequeue();
  // Timeval data structure providing the unix time
  // [microseconds] at which the frame was captured in the ring buffer.
  timestamp = frame->timestamp;
  id = frame->id;

  this->width  = frame->size[0];
  this->height = frame->size[1];
  unsigned int size = this->width * this->height;

  if ((I.getWidth() != width)||(I.getHeight() != height))
    I.resize(height, width);

  switch (frame->color_coding) {
    case DC1394_COLOR_CODING_MONO8:
      vpImageConvert::GreyToRGBa((unsigned char *) frame->image,
                                 (unsigned char *) I.bitmap, size);
      break;

    case DC1394_COLOR_CODING_YUV411:
      vpImageConvert::YUV411ToRGBa( (unsigned char *) frame->image,
                                    (unsigned char *) I.bitmap, size);
      break;

    case DC1394_COLOR_CODING_YUV422:
      vpImageConvert::YUV422ToRGBa( (unsigned char *) frame->image,
                                    (unsigned char *) I.bitmap, size);
      break;

    case DC1394_COLOR_CODING_YUV444:
      vpImageConvert::YUV444ToRGBa( (unsigned char *) frame->image,
                                    (unsigned char *) I.bitmap, size);
      break;

    case DC1394_COLOR_CODING_RGB8:
      vpImageConvert::RGBToRGBa((unsigned char *) frame->image,
                                (unsigned char *) I.bitmap, size);
      break;


    default:
      close();
      vpERROR_TRACE("Format conversion not implemented. Acquisition failed.");
      throw (vpFrameGrabberException(vpFrameGrabberException::otherError,
                                     "Format conversion not implemented. "
                                     "Acquisition failed.") );
      break;
  };

  enqueue(frame);

}

/*!

  Get the image width. It depends on the camera video mode setVideoMode(). The
  image size is only available after a call to open() or acquire().

  \param width : The image width, zero if the required camera is not available.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \warning Has to be called after open() or acquire() to be sure that camera
  settings are send to the camera.

  \sa getHeight(), open(), acquire()

*/
void vp1394TwoGrabber::getWidth(unsigned int &width)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  width = this->width;

}

/*!

  Get the image height. It depends on the camera vide mode
  setVideoMode(). The image size is only available after a call to
  open() or acquire().

  \param height : The image width.

  \exception vpFrameGrabberException::initializationError : If no
  camera found on the bus.

  \warning Has to be called after open() or acquire() to be sure that camera
  settings are send to the camera.

  \sa getWidth()

*/
void vp1394TwoGrabber::getHeight(unsigned int &height)
{
  if (! num_cameras) {
    close();
    vpERROR_TRACE("No camera found");
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "No camera found") );
  }

  height = this->height;
}

/*!
  Display camera information for the active camera.

  \sa setCamera()
*/
void
vp1394TwoGrabber::printCameraInfo()
{
  std::cout << "----------------------------------------------------------"
  << std::endl
  << "-----            Information for camera " << camera_id
  << "            -----" << std::endl
  << "----------------------------------------------------------" << std::endl;

#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
  dc1394_camera_print_info( camera, stdout);
#elif defined VISP_HAVE_DC1394_2_FIND_CAMERAS // old API <= libdc1394-2.0.0-rc7
  dc1394_print_camera_info( camera);
#endif

  dc1394featureset_t features;
#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
  if (dc1394_feature_get_all(camera, &features) != DC1394_SUCCESS) {
#elif defined VISP_HAVE_DC1394_2_FIND_CAMERAS // old API <= libdc1394-2.0.0-rc7
  if (dc1394_get_camera_feature_set(camera, &features) != DC1394_SUCCESS) {
#endif
    close();
    vpERROR_TRACE("unable to get feature set for camera %d\n", camera_id);
    throw (vpFrameGrabberException(vpFrameGrabberException::initializationError,
                                   "Cannot get camera features") );

  } else {
#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
    dc1394_feature_print_all(&features, stdout);
#elif defined VISP_HAVE_DC1394_2_FIND_CAMERAS // old API <= libdc1394-2.0.0-rc7
    dc1394_print_feature_set(&features);
#endif
  }
  std::cout << "----------------------------------------------------------" << std::endl;
}

/*!

  Converts the video mode identifier into a string containing the description
  of the mode.

  \param videomode : The camera capture video mode.

  \return A string describing the mode, an empty string if the mode is not
  supported.

  \sa string2videoMode()
*/
std::string vp1394TwoGrabber::videoMode2string(vp1394TwoVideoModeType videomode)
{
  std::string _str = "";
  dc1394video_mode_t _videomode = (dc1394video_mode_t) videomode;

  if ((_videomode >= DC1394_VIDEO_MODE_MIN)
      && (_videomode <= DC1394_VIDEO_MODE_MAX)) {
    _str = strVideoMode[_videomode - DC1394_VIDEO_MODE_MIN];
  }
  else {
    vpCERROR << "The video mode " << videomode
    << " is not supported by the camera" << std::endl;
  }

  return _str;
}

/*!

  Converts the framerate identifier into a string containing the description
  of the framerate.

  \param fps : The camera capture framerate.

  \return A string describing the framerate, an empty string if the framerate
  is not supported.

  \sa string2framerate()
*/
std::string vp1394TwoGrabber::framerate2string(vp1394TwoFramerateType fps)
{
  std::string _str = "";
  dc1394framerate_t _fps = (dc1394framerate_t) fps;

  if ((_fps >= DC1394_FRAMERATE_MIN)
      && (_fps <= DC1394_FRAMERATE_MAX)) {
    _str = strFramerate[_fps - DC1394_FRAMERATE_MIN];
  }
  else {
    vpCERROR << "The framerate " << fps
    << " is not supported by the camera" << std::endl;
  }

  return _str;
}

/*!

  Converts the color coding identifier into a string containing the description
  of the color coding.

  \param colorcoding : The color coding format.

  \return A string describing the color coding, an empty string if the
  color coding is not supported.

  \sa string2colorCoding()
*/
std::string vp1394TwoGrabber::colorCoding2string(vp1394TwoColorCodingType colorcoding)
{
  std::string _str = "";
  dc1394color_coding_t _coding = (dc1394color_coding_t) colorcoding;

  if ((_coding >= DC1394_COLOR_CODING_MIN)
      && (_coding <= DC1394_COLOR_CODING_MAX)) {
    _str = strColorCoding[_coding - DC1394_COLOR_CODING_MIN];

  }
  else {
    vpCERROR << "The color coding " << colorcoding
    << " is not supported by the camera" << std::endl;
  }

  return _str;
}

/*!

  Converts the string containing the description of the vide mode into
  the video mode identifier.

  \param videomode : The string describing the video mode.

  \return The camera capture video mode identifier.

  \exception vpFrameGrabberException::settingError : If the required videomode
  is not valid.

  This method returns 0 if the string does not match to a video mode string.

  \sa videoMode2string()

*/
vp1394TwoGrabber::vp1394TwoVideoModeType
vp1394TwoGrabber::string2videoMode(std::string videomode)
{
  vp1394TwoVideoModeType _id;

  for (int i = DC1394_VIDEO_MODE_MIN; i <= DC1394_VIDEO_MODE_MAX; i ++) {
    _id = (vp1394TwoVideoModeType) i;
    if (videomode.compare(videoMode2string(_id)) == 0)
      return _id;
  };

  throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                 "The required videomode is not valid") );

  return (vp1394TwoVideoModeType) 0;
}


/*!

  Converts the string containing the description of the framerate into the
  framerate identifier.

  \param framerate : The string describing the framerate.

  \return The camera capture framerate identifier.

  \exception vpFrameGrabberException::settingError : If the required framerate
  is not valid.

  This method returns 0 if the string does not match to a framerate string.

  \sa framerate2string()

*/
vp1394TwoGrabber::vp1394TwoFramerateType
vp1394TwoGrabber::string2framerate(std::string framerate)
{
  vp1394TwoFramerateType _id;

  for (int i = DC1394_FRAMERATE_MIN; i <= DC1394_FRAMERATE_MAX; i ++) {
    _id = (vp1394TwoFramerateType) i;
    if (framerate.compare(framerate2string(_id)) == 0)
      return _id;
  };

  throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                 "The required framerate is not valid") );

  return (vp1394TwoFramerateType) 0;
}

/*!

  Converts the string containing the description of the color coding into the
  color coding identifier.

  \param colorcoding : The string describing the color coding format.

  \return The camera capture color coding identifier.

  \exception vpFrameGrabberException::settingError : If the required
  color coding is not valid.

  This method returns 0 if the string does not match to a color coding string.

  \sa colorCoding2string()

*/
vp1394TwoGrabber::vp1394TwoColorCodingType
vp1394TwoGrabber::string2colorCoding(std::string colorcoding)
{
  vp1394TwoColorCodingType _id;

  for (int i = DC1394_COLOR_CODING_MIN; i <= DC1394_COLOR_CODING_MAX; i ++) {
    _id = (vp1394TwoColorCodingType) i;
    if (colorcoding.compare(colorCoding2string(_id)) == 0)
      return _id;
  };

  throw (vpFrameGrabberException(vpFrameGrabberException::settingError,
                                 "The required color coding is not valid") );

  return (vp1394TwoColorCodingType) 0;
}

/*!
  Resets the IEEE1394 bus which camera is attached to.  Calling this function is
  "rude" to other devices because it causes them to re-enumerate on the bus and
  may cause a temporary disruption in their current activities.  Thus, use it
  sparingly.  Its primary use is if a program shuts down uncleanly and needs to
  free leftover ISO channels or bandwidth.  A bus reset will free those things
  as a side effect.

  The example below shows how to reset the bus attached to the last camera found.
  \code
  unsigned int ncameras; // Number of cameras on the bus
  vp1394TwoGrabber g;
  g.getNumCameras(ncameras);
  g.setCamera(ncameras-1); // To dial with the last camera on the bus
  g.resetBus(); // Reset the bus attached to "ncameras-1"
  \endcode

  \exception vpFrameGrabberException::initializationError : If no
  camera is found.

*/
void vp1394TwoGrabber::resetBus()
{
  for (unsigned int i = 0; i < num_cameras;i++) {
    if (camIsOpen[i]) {
      camera = cameras[i];
      setTransmission(DC1394_OFF);
      setCapture(DC1394_OFF);
    }
  }
#ifdef VISP_HAVE_DC1394_2_CAMERA_ENUMERATE // new API > libdc1394-2.0.0-rc7
  setCamera(camera_id);
  // free the other cameras
  for (unsigned int i=0;i<num_cameras;i++){
    if (i!=camera_id) dc1394_camera_free(cameras[i]);
  }
  if (list != NULL){
    dc1394_camera_free_list (list);
    list = NULL;
  }

  printf ("Reseting bus...\n");
  dc1394_reset_bus (camera);

  dc1394_camera_free (camera);
  dc1394_free (d);
  d = NULL;
  if (cameras != NULL)
    delete [] cameras;
  cameras = NULL ;
#elif defined VISP_HAVE_DC1394_2_FIND_CAMERAS // old API <= libdc1394-2.0.0-rc7

  setCamera(camera_id);
  // free the other cameras
  for (unsigned int i=0;i<num_cameras;i++){
    if (i!=camera_id) dc1394_free_camera(cameras[i]);
  }
  free(cameras);
  cameras = NULL;

  dc1394_reset_bus(camera);
  dc1394_free_camera(camera);

#endif
  if (camIsOpen != NULL)
    delete [] camIsOpen;
  camIsOpen = NULL ;

  num_cameras = 0;

  init = false;
  vpTime::wait(1000);
  initialize();
}


#endif

