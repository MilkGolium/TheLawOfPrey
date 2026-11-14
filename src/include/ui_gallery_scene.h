#ifndef UI_GALLERY_SCENE_H
#define UI_GALLERY_SCENE_H

// 图鉴场景的页签状态。页签切换由场景持有，绘制原语本身不保存跨帧状态。
// 这两个入口供回归测试直接定位页面，避免测试只能靠鼠标事件切页。

// 设置当前页，钳到 0..5。
void UiGallerySetPage(int page);

// 当前页下标。
int UiGalleryGetPage(void);

#endif  // UI_GALLERY_SCENE_H
