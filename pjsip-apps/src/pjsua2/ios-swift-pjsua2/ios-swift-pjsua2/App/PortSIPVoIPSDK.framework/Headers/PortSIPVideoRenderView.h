
#import <UIKit/UIKit.h>

/*!
 * @author Copyright (c) 2006-2021 PortSIP Solutions,Inc. All rights reserved.
 * @version 18
 * @see http://www.PortSIP.com
 * @class PortSIPVideoRenderView
 * @brief PortSIP VoIP SDK Video Render View class.
 
 PortSIP VoIP SDK Video Render View class description.
 */
@interface PortSIPVideoRenderView : UIView

/*!
 *  @brief Initialize the Video Render view. Render should be initialized before using.
 */
- (void)initVideoRender;

/*!
 *  @brief Release the Video Render.
 */
- (void)releaseVideoRender;

/*!
 *  @brief Don't use this. Just call by SDK.
 */
-(void*)getVideoRenderView;

/*!
 *  @brief Change the Video Render size.
 @remark Example:
 @code
 NSRect rect = videoRenderView.frame;
 rect.size.width += 20;
 rect.size.height += 20;
 
 videoRenderView.frame = rect;
 [videoRenderView setNeedsDisplay:YES];
 
 NSRect renderRect = [videoRenderView bounds];
 [videoRenderView updateVideoRenderFrame:renderRect];
 @endcode
 */
-(void)updateVideoRenderFrame:(CGRect)frameRect;
@end
