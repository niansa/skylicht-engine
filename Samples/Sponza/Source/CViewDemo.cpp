#include "pch.h"
#include "CViewDemo.h"

#include "Context/CContext.h"

CViewDemo::CViewDemo()
{

}

CViewDemo::~CViewDemo()
{

}

void CViewDemo::onInit()
{
	CContext *context = CContext::getInstance();
	CCamera *camera = context->getActiveCamera();
}

void CViewDemo::onDestroy()
{

}

void CViewDemo::onUpdate()
{
	CContext *context = CContext::getInstance();
	CScene *scene = context->getScene();
	if (scene != NULL)
		scene->update();
}

bool bake = false;

void CViewDemo::onRender()
{
	CContext *context = CContext::getInstance();

	CCamera *camera = context->getActiveCamera();
	CCamera *guiCamera = context->getGUICamera();

	CScene *scene = context->getScene();

	// render scene
	if (camera != NULL && scene != NULL)
	{
		context->updateDirectionLight();

		context->getRenderPipeline()->render(NULL, camera, scene->getEntityManager(), core::recti());
	}

	// render GUI
	if (guiCamera != NULL)
	{
		CGraphics2D::getInstance()->render(guiCamera);
	}

	// test bake bake irradiance
	if (bake == false)
	{
		CGameObject *bakeCameraObj = context->getActiveZone()->createEmptyObject();
		CCamera *bakeCamera = bakeCameraObj->addComponent<CCamera>();
		scene->updateAddRemoveObject();

		CLightmapper::getInstance()->initBaker(32);

		CLightmapper::getInstance()->bakeProbes(
			context->getProbes(),
			bakeCamera,
			context->getRenderPipeline(),
			scene->getEntityManager()
		);

		bakeCameraObj->remove();
		bake = true;
	}
}

void CViewDemo::onPostRender()
{

}
