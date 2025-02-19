/*
!@
MIT License

Copyright (c) 2020 Skylicht Technology CO., LTD

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction, including without limitation the Rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

This file is part of the "Skylicht Engine".
https://github.com/skylicht-lab/skylicht-engine
!#
*/

#include "pch.h"
#include "CSpaceScene.h"
#include "GUI/Input/CInput.h"
#include "GridPlane/CGridPlane.h"
#include "SpriteDraw/CSprite.h"

#include "Editor/CEditor.h"
#include "Handles/CHandles.h"
#include "Selection/CSelection.h"
#include "Selection/CSelecting.h"
#include "Projective/CProjective.h"

#include "Editor/SpaceController/CSceneController.h"

#include "Editor/Gizmos/Transform/CTransformGizmos.h"

using namespace std::placeholders;

namespace Skylicht
{
	namespace Editor
	{
		CSpaceScene::CSpaceScene(GUI::CWindow* window, CEditor* editor) :
			CSpace(window, editor),
			m_scene(NULL),
			m_renderRP(NULL),
			m_viewpointRP(NULL),
			m_editorCamera(NULL),
			m_gridPlane(NULL),
			m_leftMouseDown(false),
			m_rightMouseDown(false),
			m_middleMouseDown(false),
			m_mouseX(0.0f),
			m_mouseY(0.0f),
			m_viewpointZone(NULL),
			m_viewpoint(NULL),
			m_viewpointCamera(NULL),
			m_viewpointController(NULL),
			m_view(NULL),
			m_handlesRenderer(NULL),
			m_gizmosRenderer(NULL),
			m_selectObjectSystem(NULL),
			m_enableRender(true)
		{
			initDefaultScene();

			GUI::CToolbar* toolbar = new GUI::CToolbar(window);
			m_groupEditor = new GUI::CToggleGroup();
			m_groupTransform = new GUI::CToggleGroup();
			m_groupCameraView = new GUI::CToggleGroup();

			GUI::CButton* button = NULL;

			// select & hand
			button = toolbar->addButton(L"Select", GUI::ESystemIcon::ViewSelect);
			button->OnPress = BIND_LISTENER(&CSpaceScene::onEditorSelect, this);
			m_toolbarButton[ESceneToolBar::Select] = button;
			m_groupEditor->addButton(button);

			button = toolbar->addButton(L"Hand", GUI::ESystemIcon::ViewHand);
			button->OnPress = BIND_LISTENER(&CSpaceScene::onEditorHand, this);
			m_toolbarButton[ESceneToolBar::Hand] = button;
			m_groupEditor->addButton(button);

			toolbar->addSpace();

			// transform			
			button = toolbar->addButton(L"Move", GUI::ESystemIcon::Move);
			button->OnPress = BIND_LISTENER(&CSpaceScene::onToolbarTransform, this);
			m_toolbarButton[ESceneToolBar::Move] = button;
			m_groupTransform->addButton(button);

			button = toolbar->addButton(L"Rotate", GUI::ESystemIcon::Rotate);
			button->OnPress = BIND_LISTENER(&CSpaceScene::onToolbarTransform, this);
			m_toolbarButton[ESceneToolBar::Rotate] = button;
			m_groupTransform->addButton(button);

			button = toolbar->addButton(L"Scale", GUI::ESystemIcon::Scale);
			button->OnPress = BIND_LISTENER(&CSpaceScene::onToolbarTransform, this);
			m_toolbarButton[ESceneToolBar::Scale] = button;
			m_groupTransform->addButton(button);

			toolbar->addSpace();

			GUI::CComboBox* worldSpaceDropDown = (GUI::CComboBox*)toolbar->addControl(new GUI::CComboBox(toolbar));
			worldSpaceDropDown->setWidth(120.0f);
			worldSpaceDropDown->addItem(L"Local space");
			worldSpaceDropDown->addItem(L"World space");
			worldSpaceDropDown->setSelectIndex(0, false);
			worldSpaceDropDown->setDisabled(true);
			worldSpaceDropDown->OnChanged = BIND_LISTENER(&CSpaceScene::onToolbarWorldSpace, this);

			m_toolbarButton[World] = worldSpaceDropDown;

			CTransformGizmos::getGizmosSubject()->addObserver(this);

			// camera			
			button = toolbar->addButton(L"Ortho", GUI::ESystemIcon::Ortho, true);
			button->OnPress = BIND_LISTENER(&CSpaceScene::onCameraOrtho, this);
			m_toolbarButton[ESceneToolBar::Ortho] = button;
			m_groupCameraView->addButton(button);

			button = toolbar->addButton(L"Perspective", GUI::ESystemIcon::Perspective, true);
			button->OnPress = BIND_LISTENER(&CSpaceScene::onCameraPerspective, this);
			m_toolbarButton[ESceneToolBar::Perspective] = button;
			m_groupCameraView->addButton(button);

			button = toolbar->addButton(L"Camera", GUI::ESystemIcon::Camera, true);

			// init group
			m_groupEditor->enable(true);
			m_groupEditor->selectButton(m_toolbarButton[ESceneToolBar::Select]);

			m_groupCameraView->enable(true);
			m_groupCameraView->selectButton(m_toolbarButton[ESceneToolBar::Perspective]);

			m_groupTransform->selectButton(m_toolbarButton[ESceneToolBar::Move]);
			m_groupTransform->enable(false);

			// 3d view
			m_view = new GUI::CBase(window);
			m_view->dock(GUI::EPosition::Fill);

			m_view->OnRender = BIND_LISTENER(&CSpaceScene::onRender, this);
			m_view->OnMouseMoved = std::bind(&CSpaceScene::onMouseMoved, this, _1, _2, _3, _4, _5);
			m_view->OnLeftMouseClick = std::bind(&CSpaceScene::onLeftMouseClick, this, _1, _2, _3, _4);
			m_view->OnRightMouseClick = std::bind(&CSpaceScene::onRightMouseClick, this, _1, _2, _3, _4);
			m_view->OnMiddleMouseClick = std::bind(&CSpaceScene::onMiddleMouseClick, this, _1, _2, _3, _4);
			m_view->OnMouseWheeled = std::bind(&CSpaceScene::onMouseWheeled, this, _1, _2);

			m_view->setKeyboardInputEnabled(true);
			m_view->OnKeyPress = std::bind(&CSpaceScene::onKeyPressed, this, _1, _2, _3);
			m_view->OnChar = std::bind(&CSpaceScene::onChar, this, _1, _2);

			m_view->OnResize = BIND_LISTENER(&CSpaceScene::onRenderResize, this);

			GUI::SDimension size = window->getSize();
			initRenderPipeline(size.Width, size.Height);

			CSceneController::getInstance()->setSpaceScene(this);
		}

		CSpaceScene::~CSpaceScene()
		{
			if (m_scene != NULL)
				delete m_scene;

			if (m_renderRP != NULL)
				delete m_renderRP;

			if (m_viewpointRP != NULL)
				delete m_viewpointRP;

			if (m_viewpointController != NULL)
				delete m_viewpointController;

			delete m_groupEditor;
			delete m_groupTransform;
			delete m_groupCameraView;

			CTransformGizmos::getGizmosSubject()->removeObserver(this);

			CSceneController* sceneController = CSceneController::getInstance();
			sceneController->setScene(NULL);
			sceneController->setSpaceScene(NULL);
		}

		void CSpaceScene::initEditorObject(CZone* zone)
		{
			// create editor camera
			CGameObject* camObj = zone->createEmptyObject();
			camObj->setName(L"EditorCamera");
			camObj->setEditorObject(true);
			camObj->addComponent<CCamera>();
			camObj->addComponent<CEditorCamera>()->setMoveSpeed(2.0f);

			m_editorCamera = camObj->getComponent<CCamera>();
			m_editorCamera->setPosition(core::vector3df(-2.0f, 1.5f, -2.0f));
			m_editorCamera->lookAt(core::vector3df(0.0f, 0.0f, 0.0f), core::vector3df(0.0f, 1.0f, 0.0f));

			// grid
			m_gridPlane = zone->createEmptyObject();
			m_gridPlane->setName(L"Grid3D");
			m_gridPlane->setEditorObject(true);
			m_gridPlane->addComponent<CGridPlane>();


			// viewpoint zone
			m_viewpointZone = m_scene->createZone();
			m_viewpointZone->setName(L"ViewpointZone");
			m_viewpointZone->setEditorObject(true);

			// camera
			camObj = m_viewpointZone->createEmptyObject();
			camObj->setName(L"ViewpointCamera");
			camObj->setEditorObject(true);
			camObj->addComponent<CCamera>();

			m_viewpointCamera = camObj->getComponent<CCamera>();
			m_viewpointCamera->setAspect(1.0f);

			// viewpoint
			CGameObject* viewpoint = m_viewpointZone->createEmptyObject();
			viewpoint->setName(L"Viewpoint");
			m_viewpoint = viewpoint->addComponent<CViewpoint>();

			// delete old viewpoint
			if (m_viewpointController != NULL)
				delete m_viewpointController;

			m_viewpointController = new CViewpointController();
			m_viewpointController->setCamera(m_editorCamera, m_viewpointCamera);

			// add handle renderer
			CEntityManager* entityMgr = m_scene->getEntityManager();

			m_selectObjectSystem = entityMgr->addSystem<CSelectObjectSystem>();

			m_handlesRenderer = entityMgr->addRenderSystem<CHandlesRenderer>();
			m_gizmosRenderer = entityMgr->addRenderSystem<CGizmosRenderer>();

			// register event
			m_scene->registerEvent("Handles", CHandles::getInstance());
			m_scene->registerEvent("Selecting", CSelecting::getInstance());
		}

		CScene* CSpaceScene::initNullScene()
		{
			if (m_scene != NULL)
				delete m_scene;

			m_scene = new CScene();

			m_viewpointController = NULL;

			return m_scene;
		}

		void CSpaceScene::initDefaultScene()
		{
			// create a scene
			m_scene = new CScene();

			// create a zone in scene
			CZone* zone = m_scene->createZone();

			initEditorObject(zone);

			// lighting
			CGameObject* lightObj = zone->createEmptyObject();
			lightObj->setName(L"DirectionLight");

			CTransformEuler* lightTransform = lightObj->getTransformEuler();
			lightTransform->setPosition(core::vector3df(2.0f, 2.0f, 2.0f));

			core::vector3df direction = core::vector3df(0.0f, -1.5f, 2.0f);
			lightTransform->setOrientation(direction, CTransform::s_oy);

			CDirectionalLight* directionalLight = lightObj->addComponent<CDirectionalLight>();
			SColor c(255, 255, 244, 214);
			directionalLight->setColor(SColorf(c));

			// update search index
			m_scene->updateAddRemoveObject();
			m_scene->updateIndexSearchObject();

			// set scene to controller
			CSceneController::getInstance()->setScene(m_scene);
		}

		void CSpaceScene::onResize(float w, float h)
		{
			initRenderPipeline(w, h);
		}

		void CSpaceScene::onRenderResize(GUI::CBase* base)
		{
			initRenderPipeline(base->getSize().Width, base->getSize().Height);
		}

		// tool bar
		void CSpaceScene::onCameraPerspective(GUI::CBase* base)
		{
			m_groupCameraView->selectButton(m_toolbarButton[ESceneToolBar::Perspective]);
			if (m_editorCamera != NULL)
				m_editorCamera->setProjectionType(CCamera::Perspective);
		}

		void CSpaceScene::onCameraOrtho(GUI::CBase* base)
		{
			m_groupCameraView->selectButton(m_toolbarButton[ESceneToolBar::Ortho]);
			if (m_editorCamera != NULL)
				m_editorCamera->setProjectionType(CCamera::Ortho);
		}

		void CSpaceScene::onEditorSelect(GUI::CBase* base)
		{
			if (CSelection::getInstance()->getSelected().size() == 0)
			{
				m_groupTransform->enable(false);
				m_toolbarButton[World]->setDisabled(true);
			}
			else
			{
				m_groupTransform->enable(true);
				m_toolbarButton[World]->setDisabled(false);

				GUI::CButton* button = m_groupTransform->getSelectButton();
				if (button != NULL)
				{
					CSubject<CTransformGizmos::ETransformGizmo>* gizmos = CTransformGizmos::getGizmosSubject();
					if (button == m_toolbarButton[ESceneToolBar::Move])
						gizmos->set(CTransformGizmos::Translate);
					else if (button == m_toolbarButton[ESceneToolBar::Rotate])
						gizmos->set(CTransformGizmos::Rotate);
					else if (button == m_toolbarButton[ESceneToolBar::Scale])
						gizmos->set(CTransformGizmos::Scale);
					gizmos->notify(this);
				}
			}

			m_groupEditor->selectButton(m_toolbarButton[ESceneToolBar::Select]);
			m_view->setCursor(GUI::ECursorType::Normal);
		}

		void CSpaceScene::onEditorHand(GUI::CBase* base)
		{
			m_groupTransform->enable(false);
			m_toolbarButton[World]->setDisabled(true);

			CSubject<CTransformGizmos::ETransformGizmo>* gizmos = CTransformGizmos::getGizmosSubject();
			gizmos->set(CTransformGizmos::None);
			gizmos->notify(this);

			m_groupEditor->selectButton(m_toolbarButton[ESceneToolBar::Hand]);
			m_view->setCursor(GUI::ECursorType::Finger);
		}

		void CSpaceScene::onToolbarTransform(GUI::CBase* base)
		{
			CSubject<CTransformGizmos::ETransformGizmo>* gizmos = CTransformGizmos::getGizmosSubject();
			CTransformGizmos::ETransformGizmo type = gizmos->get();

			if (base == m_toolbarButton[ESceneToolBar::Move] && type != CTransformGizmos::Translate)
			{
				gizmos->set(CTransformGizmos::Translate);
				gizmos->notify(this);
			}
			else if (base == m_toolbarButton[ESceneToolBar::Rotate] && type != CTransformGizmos::Rotate)
			{
				gizmos->set(CTransformGizmos::Rotate);
				gizmos->notify(this);
			}
			else if (base == m_toolbarButton[ESceneToolBar::Scale] && type != CTransformGizmos::Scale)
			{
				gizmos->set(CTransformGizmos::Scale);
				gizmos->notify(this);
			}

			m_groupTransform->selectButton(dynamic_cast<GUI::CButton*>(base));
		}

		void CSpaceScene::onToolbarWorldSpace(GUI::CBase* base)
		{
			CHandles* handles = CHandles::getInstance();
			GUI::CComboBox* btn = (GUI::CComboBox*)base;
			if (btn->getSelectIndex() == 0)
				handles->setUseLocalSpace(true);
			else
				handles->setUseLocalSpace(false);
			m_editor->refresh();
		}

		void CSpaceScene::onNotify(ISubject* subject, IObserver* from)
		{
			if (from == this)
				return;

			CSubject<CTransformGizmos::ETransformGizmo>* gizmos = CTransformGizmos::getGizmosSubject();
			if (subject == gizmos)
			{
				CTransformGizmos::ETransformGizmo type = gizmos->get();
				switch (type)
				{
				case CTransformGizmos::None:
					m_groupTransform->enable(false);
					m_toolbarButton[World]->setDisabled(true);
					break;
				case CTransformGizmos::Translate:
					m_groupTransform->selectButton(m_toolbarButton[ESceneToolBar::Move]);
					m_groupTransform->enable(true);
					m_toolbarButton[World]->setDisabled(false);
					break;
				case CTransformGizmos::Rotate:
					m_groupTransform->selectButton(m_toolbarButton[ESceneToolBar::Rotate]);
					m_groupTransform->enable(true);
					m_toolbarButton[World]->setDisabled(false);
					break;
				case CTransformGizmos::Scale:
					m_groupTransform->selectButton(m_toolbarButton[ESceneToolBar::Scale]);
					m_groupTransform->enable(true);
					m_toolbarButton[World]->setDisabled(false);
					break;
				default:
					break;
				}
			}
		}

		// end toolbar

		void CSpaceScene::initRenderPipeline(float w, float h)
		{
			if (w >= 32.0f && h >= 32.0f)
			{
				if (m_renderRP == NULL)
				{
					// rendering pipe line
					m_renderRP = new CForwardRP();
					m_renderRP->initRender((int)w, (int)h);
				}
				else
				{
					// resize
					m_renderRP->resize((int)w, (int)h);
				}

				if (m_viewpointRP == NULL)
				{
					// rendering pipe line
					m_viewpointRP = new CForwardRP(false);
					m_viewpointRP->initRender((int)w, (int)h);
				}
				else
				{
					m_viewpointRP->resize((int)w, (int)h);
				}

				// update camera aspect
				m_editorCamera->setAspect(w / h);
			}
		}

		void CSpaceScene::update()
		{
			// update controller
			CSceneController::getInstance()->update();

			// update viewpoint
			if (m_viewpointController != NULL)
				m_viewpointController->update();

			// update scene
			m_scene->update();
		}

		void CSpaceScene::refresh()
		{
			CSceneController::getInstance()->refresh();
		}

		void CSpaceScene::onRender(GUI::CBase* base)
		{
			if (m_renderRP != NULL)
			{
				// flush 2d gui
				GUI::CGUIContext::getRenderer()->flush();
				core::recti vp = getVideoDriver()->getViewPort();
				getVideoDriver()->enableScissor(false);
				getVideoDriver()->clearZBuffer();

				if (m_enableRender)
				{
					// setup scene viewport
					GUI::SPoint position = base->localPosToCanvas();
					core::recti viewport;
					viewport.UpperLeftCorner.set((int)position.X, (int)position.Y);
					viewport.LowerRightCorner.set((int)(position.X + base->width()), (int)(position.Y + base->height()));
					m_sceneRect = viewport;

					// draw scene
					m_scene->setVisibleAllZone(true);
					m_viewpointZone->setVisible(false);

					m_handlesRenderer->setEnable(true);
					m_handlesRenderer->setCameraAndViewport(m_editorCamera, viewport);

					m_selectObjectSystem->setCameraAndViewport(m_editorCamera, viewport);

					m_gizmosRenderer->setEnable(true);

					m_renderRP->render(NULL, m_editorCamera, m_scene->getEntityManager(), viewport);

					// setup viewpoint viewport
					int paddingTop = 10;
					int paddingLeft = 10;
					int viewpointSize = 125;
					viewport.UpperLeftCorner.set((int)(position.X + base->width()) - viewpointSize - paddingLeft, (int)position.Y + paddingTop);
					viewport.LowerRightCorner = viewport.UpperLeftCorner + core::vector2di(viewpointSize, viewpointSize);

					// viewpoint rect (local)
					GUI::SPoint upleft = m_view->canvasPosToLocal(GUI::SPoint((float)viewport.UpperLeftCorner.X, (float)viewport.UpperLeftCorner.Y));
					GUI::SPoint lowerRight = m_view->canvasPosToLocal(GUI::SPoint((float)viewport.LowerRightCorner.X, (float)viewport.LowerRightCorner.Y));
					m_viewpointRect.UpperLeftCorner.set(upleft.X, upleft.Y);
					m_viewpointRect.LowerRightCorner.set(lowerRight.X, lowerRight.Y);
					m_viewpointRect.repair();

					getVideoDriver()->clearZBuffer();

					// draw viewpoint
					m_scene->setVisibleAllZone(false);
					m_viewpointZone->setVisible(true);
					m_handlesRenderer->setEnable(false);
					m_gizmosRenderer->setEnable(false);

					m_viewpointRP->render(NULL, m_viewpointCamera, m_scene->getEntityManager(), viewport);

					// disable viewpoint
					m_scene->setVisibleAllZone(true);
					m_viewpointZone->setVisible(false);
					m_handlesRenderer->setEnable(true);
				}
				else
				{

				}

				// resume gui render
				getVideoDriver()->enableScissor(true);
				getVideoDriver()->setViewPort(vp);
				GUI::CGUIContext::getRenderer()->setProjection();
			}
		}

		void CSpaceScene::onMouseMoved(GUI::CBase* base, float x, float y, float deltaX, float deltaY)
		{
			GUI::SPoint local = m_view->canvasPosToLocal(GUI::SPoint(x, y));
			postMouseEventToScene(EMIE_MOUSE_MOVED, local.X, local.Y);
		}

		void CSpaceScene::onLeftMouseClick(GUI::CBase* base, float x, float y, bool down)
		{
			m_leftMouseDown = down;
			GUI::SPoint local = m_view->canvasPosToLocal(GUI::SPoint(x, y));

			if (m_viewpointRect.isPointInside(core::vector2df(local.X, local.Y)))
			{
				// viewpoint
				if (down)
				{
					float x = local.X - m_viewpointRect.UpperLeftCorner.X;
					float y = local.Y - m_viewpointRect.UpperLeftCorner.Y;
					CViewpointData::EAxis axis = m_viewpoint->getViewpointData()->hit(m_viewpointCamera, x, y, (int)m_viewpointRect.getWidth(), (int)m_viewpointRect.getHeight());

					if (axis != CViewpointData::None)
						m_viewpointController->setCameraLook(axis);
				}
			}
			else
			{
				// scene
				if (down)
				{
					GUI::CInput::getInput()->setCapture(m_view);
					postMouseEventToScene(EMIE_LMOUSE_PRESSED_DOWN, local.X, local.Y);
				}
				else
				{
					GUI::CInput::getInput()->setCapture(NULL);
					postMouseEventToScene(EMIE_LMOUSE_LEFT_UP, local.X, local.Y);

					// try test selection
					// core::line3df ray = CProjective::getViewRay(m_editorCamera, local.X, local.Y, m_sceneRect.getWidth(), m_sceneRect.getHeight());
				}
			}
		}

		void CSpaceScene::onRightMouseClick(GUI::CBase* base, float x, float y, bool down)
		{
			m_rightMouseDown = down;
			GUI::SPoint local = m_view->canvasPosToLocal(GUI::SPoint(x, y));

			if (m_viewpointRect.isPointInside(core::vector2df(local.X, local.Y)))
			{
				// viewpoint
			}
			else
			{
				// scene
				if (down)
				{
					GUI::CInput::getInput()->setCapture(m_view);
					postMouseEventToScene(EMIE_RMOUSE_PRESSED_DOWN, local.X, local.Y);
				}
				else
				{
					GUI::CInput::getInput()->setCapture(NULL);
					postMouseEventToScene(EMIE_RMOUSE_LEFT_UP, local.X, local.Y);
				}
			}
		}

		void CSpaceScene::onMiddleMouseClick(GUI::CBase* base, float x, float y, bool down)
		{
			m_middleMouseDown = down;
			GUI::SPoint local = m_view->canvasPosToLocal(GUI::SPoint(x, y));

			if (m_viewpointRect.isPointInside(core::vector2df(local.X, local.Y)))
			{
				// viewpoint
			}
			else
			{
				// scene
				if (down)
				{
					GUI::CInput::getInput()->setCapture(m_view);
					postMouseEventToScene(EMIE_MMOUSE_PRESSED_DOWN, local.X, local.Y);
				}
				else
				{
					GUI::CInput::getInput()->setCapture(NULL);
					postMouseEventToScene(EMIE_MMOUSE_LEFT_UP, local.X, local.Y);
				}
			}
		}

		void CSpaceScene::onMouseWheeled(GUI::CBase* base, int wheel)
		{
			if (m_viewpointRect.isPointInside(core::vector2df(m_mouseX, m_mouseY)))
			{
				// viewpoint
			}
			else
			{
				// scene
				SEvent event;
				event.EventType = EET_MOUSE_INPUT_EVENT;
				event.MouseInput.Event = EMIE_MOUSE_WHEEL;
				event.MouseInput.X = (int)m_mouseX;
				event.MouseInput.Y = (int)m_mouseY;
				event.MouseInput.Wheel = (float)wheel;
				event.MouseInput.ButtonStates = 0;

				if (m_leftMouseDown)
					event.MouseInput.ButtonStates |= EMBSM_LEFT;

				if (m_rightMouseDown)
					event.MouseInput.ButtonStates |= EMBSM_RIGHT;

				event.GameEvent.Sender = m_scene;

				m_scene->OnEvent(event);
			}
		}

		void CSpaceScene::postMouseEventToScene(EMOUSE_INPUT_EVENT eventType, float x, float y)
		{
			m_mouseX = x;
			m_mouseY = y;

			SEvent event;
			event.EventType = EET_MOUSE_INPUT_EVENT;
			event.MouseInput.Event = eventType;
			event.MouseInput.X = (int)m_mouseX;
			event.MouseInput.Y = (int)m_mouseY;
			event.MouseInput.Wheel = 0.0f;
			event.MouseInput.ButtonStates = 0;

			if (m_leftMouseDown)
				event.MouseInput.ButtonStates |= EMBSM_LEFT;

			if (m_rightMouseDown)
				event.MouseInput.ButtonStates |= EMBSM_RIGHT;

			event.GameEvent.Sender = m_scene;

			m_scene->OnEvent(event);
		}

		void CSpaceScene::onChar(GUI::CBase* base, u32 c)
		{

		}

		void CSpaceScene::onKeyPressed(GUI::CBase* base, int key, bool down)
		{
			GUI::CInput* input = GUI::CInput::getInput();

			SEvent event;
			event.EventType = EET_KEY_INPUT_EVENT;
			event.KeyInput.Key = (irr::EKEY_CODE)key;
			event.KeyInput.PressedDown = down;
			event.KeyInput.Control = input->IsControlDown();
			event.KeyInput.Shift = input->IsShiftDown();

			event.GameEvent.Sender = m_scene;

			m_scene->OnEvent(event);
		}

		bool CSpaceScene::isEditorObject(CGameObject* object)
		{
			if (m_editorCamera->getGameObject() == object ||
				m_gridPlane == object)
			{
				return true;
			}

			return false;
		}
	}
}