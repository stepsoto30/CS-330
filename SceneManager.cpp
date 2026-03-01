///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ============
// manage the preparing and rendering of 3D scenes - textures, materials, lighting
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();

	// initialize the texture collection
	for (int i = 0; i < 16; i++)
	{
		m_textureIDs[i].tag = "/0";
		m_textureIDs[i].ID = -1;
	}
	m_loadedTextures = 0;
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	// clear the allocated memory
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
	// destroy the created OpenGL textures
	DestroyGLTextures();
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glGenTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationZ * rotationY * rotationX * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/


/***********************************************************
  *  LoadSceneTextures()
  *
  *  This method is used for preparing the 3D scene by loading
  *  the shapes, textures in memory to support the 3D scene
  *  rendering
  ***********************************************************/
void SceneManager::LoadSceneTextures()
{
	/*** STUDENTS - add the code BELOW for loading the textures that ***/
	/*** will be used for mapping to objects in the 3D scene. Up to  ***/
	/*** 16 textures can be loaded per scene. Refer to the code in   ***/
	/*** the OpenGL Sample for help.                                 ***/

	bool bReturn = false;

	// texture for the landings and ladder
	bReturn = CreateGLTexture(
		"textures/wood.jpg",
		"wood");

	// texture for the box and the perches
	bReturn = CreateGLTexture(
		"textures/pink4.jpg",
		"pink");

	// texture for the base
	bReturn = CreateGLTexture(
		"textures/pink4.jpg",
		"base");
	
	// texture for the poles
	bReturn = CreateGLTexture(
		"textures/scratch.jpg",
		"pole");

	// texture for the wall
	bReturn = CreateGLTexture(
		"textures/wall2.jpg",
		"wall");

	// texture for the flooring
	bReturn = CreateGLTexture(
		"textures/floor.jpg",
		"floor");

	// texture for the ball
	bReturn = CreateGLTexture(
		"textures/fur.jpg",
		"fur");

	// texture for the string attached to the ball
	bReturn = CreateGLTexture(
		"textures/paper.jpg",
		"holder");

	// texture for the top of the entrance of the box
	bReturn = CreateGLTexture(
		"textures/carpet3.jpg",
		"top");

	// texture for the bottom of the entrance of the box
	bReturn = CreateGLTexture(
		"textures/carpet4.jpg",
		"bottom");

	// texture for the future cat
	bReturn = CreateGLTexture(
		"textures/cat.jpg",
		"cat");

	// texture for the food bowl
	bReturn = CreateGLTexture(
		"textures/stone.jpg",
		"bowl");

	// after the texture image data is loaded into memory, the
	// loaded textures need to be bound to texture slots - there
	// are a total of 16 available slots for scene textures
	BindGLTextures();
}

/***********************************************************
 *  DefineObjectMaterials()
 *
 *  This method is used for configuring the various material
 *  settings for all of the objects within the 3D scene.
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
		// Realistic wood
		OBJECT_MATERIAL woodMaterial;
		woodMaterial.diffuseColor = glm::vec3(0.65f, 0.55f, 0.45f);
		woodMaterial.specularColor = glm::vec3(0.15f, 0.12f, 0.1f);
		woodMaterial.shininess = 8.0f;
		woodMaterial.tag = "wood";
		m_objectMaterials.push_back(woodMaterial);

		// Bright pink
		OBJECT_MATERIAL pinkMaterial;
		pinkMaterial.diffuseColor = glm::vec3(1.2f, 1.2f, 1.2f);
		pinkMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
		pinkMaterial.shininess = 8.0f;
		pinkMaterial.tag = "pink";
		m_objectMaterials.push_back(pinkMaterial);

		// Realistic scratching pole
		OBJECT_MATERIAL poleMaterial;
		poleMaterial.diffuseColor = glm::vec3(1.1f, 1.1f, 1.1f);
		poleMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.1f);
		poleMaterial.shininess = 4.0f;
		poleMaterial.tag = "pole";
		m_objectMaterials.push_back(poleMaterial);

		// Bright wall
		OBJECT_MATERIAL wallMaterial;
		wallMaterial.diffuseColor = glm::vec3(0.95f, 0.95f, 0.95f);
		wallMaterial.specularColor = glm::vec3(0.08f, 0.08f, 0.08f);
		wallMaterial.shininess = 6.0f;
		wallMaterial.tag = "wall";
		m_objectMaterials.push_back(wallMaterial);

		// Bright, realistic floor
		OBJECT_MATERIAL floorMaterial;
		floorMaterial.diffuseColor = glm::vec3(1.3f, 1.3f, 1.3f);
		floorMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
		floorMaterial.shininess = 8.0f;
		floorMaterial.tag = "floor";
		m_objectMaterials.push_back(floorMaterial);

		// Realistic wood that is lighter from the wood landing
		OBJECT_MATERIAL ladderMaterial; 
		ladderMaterial.diffuseColor = glm::vec3(0.75f, 0.75f, 0.75f); 
		ladderMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.2f); 
		ladderMaterial.shininess = 10.0f; ladderMaterial.tag = "ladder"; 
		m_objectMaterials.push_back(ladderMaterial);

		// Lighter pink for the base
		OBJECT_MATERIAL baseMaterial; 
		baseMaterial.diffuseColor = glm::vec3(0.75f, 0.75f, 0.75f); 
		baseMaterial.specularColor = glm::vec3(0.15f, 0.15f, 0.15f); 
		baseMaterial.shininess = 6.0f; 
		baseMaterial.tag = "base"; 
		m_objectMaterials.push_back(baseMaterial);

		// The top half of the entrance 
		OBJECT_MATERIAL topMaterial;
		topMaterial.diffuseColor = glm::vec3(1.2f, 1.1f, 1.05f);
		topMaterial.specularColor = glm::vec3(0.15f, 0.15f, 0.15f);
		topMaterial.shininess = 10.0f;
		topMaterial.tag = "top";
		m_objectMaterials.push_back(topMaterial);

		// The bottom half of the entrance
		OBJECT_MATERIAL bottomMaterial;
		bottomMaterial.diffuseColor = glm::vec3(1.4f, 1.32f, 1.28f);
		bottomMaterial.specularColor = glm::vec3(0.22f, 0.22f, 0.22f);
		bottomMaterial.shininess = 14.0f;
		bottomMaterial.tag = "bottom";
		m_objectMaterials.push_back(bottomMaterial);

		// Realistic fur for the cat
		OBJECT_MATERIAL catMaterial;
		catMaterial.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
		catMaterial.specularColor = glm::vec3(0.05f, 0.05f, 0.05f);
		catMaterial.shininess = 4.0f;
		catMaterial.tag = "cat";
		m_objectMaterials.push_back(catMaterial);

		// Realistic stone cat bowl
		OBJECT_MATERIAL bowlMaterial;
		bowlMaterial.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
		bowlMaterial.specularColor = glm::vec3(0.18f, 0.18f, 0.18f);
		bowlMaterial.shininess = 24.0f;
		bowlMaterial.tag = "bowl";
		m_objectMaterials.push_back(bowlMaterial);
}

/***********************************************************
 *  SetupSceneLights()
 *
 *  This method is called to add and configure the light
 *  sources for the 3D scene.  There are up to 4 light sources.
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
	// this line of code is NEEDED for telling the shaders to render 
	// the 3D scene with custom lighting - to use the default rendered 
	// lighting then comment out the following line
	m_pShaderManager->setBoolValue(g_UseLightingName, true);
	
	// directional light (soft warm indoor light)
	m_pShaderManager->setVec3Value("directionalLight.direction", -0.2f, -1.0f, -0.2f);
	m_pShaderManager->setVec3Value("directionalLight.ambient", 0.38f, 0.34f, 0.32f);
	m_pShaderManager->setVec3Value("directionalLight.diffuse", 0.65f, 0.6f, 0.55f);
	m_pShaderManager->setVec3Value("directionalLight.specular", 0.25f, 0.22f, 0.2f);
	m_pShaderManager->setBoolValue("directionalLight.bActive", true);

	// point light 1
	m_pShaderManager->setVec3Value("pointLights[0].position", 2.0f, 5.0f, 2.0f); 
	m_pShaderManager->setVec3Value("pointLights[0].ambient", 0.15f, 0.15f, 0.15f); 
	m_pShaderManager->setVec3Value("pointLights[0].diffuse", 0.55f, 0.48f, 0.42f); 
	m_pShaderManager->setVec3Value("pointLights[0].specular", 0.25f, 0.22f, 0.2f); 
	m_pShaderManager->setFloatValue("pointLights[0].constant", 1.0f);
	m_pShaderManager->setFloatValue("pointLights[0].linear", 0.07f);
	m_pShaderManager->setFloatValue("pointLights[0].quadratic", 0.017f);
	m_pShaderManager->setBoolValue("pointLights[0].bActive", true);
}


/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// load the textures for the 3D scene
	LoadSceneTextures();
	// define the materials that will be used for the objects
	// in the 3D scene
	DefineObjectMaterials();
	// add and defile the light sources for the 3D scene
	SetupSceneLights();

	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene


	// Loading the plane, box, cylinder, sphere, cone, and torus mesh
	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadSphereMesh();
	m_basicMeshes->LoadConeMesh();
	m_basicMeshes->LoadTorusMesh();

}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

						/* FLOOR */
	/* This plane represents the wooden floor from the 2D reference image */
	/* Its width and depth were chosen to comfortably contain the full cat tree scene */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(20.0f, 1.0f, 10.0f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Mimicking the flooring color from the picture
	// SetShaderColor(0.47, 0.33, 0.258, 1);

	// set the texture for the next draw command
	SetShaderTexture("floor");
	// UV scale (5.0, 5.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic wood flooring look
	SetTextureUVScale(5.0, 5.0);
	SetShaderMaterial("floor");

	// draw the mesh with transformation values
	// Drawing the floor
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/

							/* WALL */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(20.0f, 1.0f, 10.0f);

	// set the XYZ rotation for the mesh
	// Rotating 90 degrees on the x-axis
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 10.0f, -10.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// set the color values into the shader
	// Changed the gray wall color to a teal tone
	// SetShaderColor(0.513, 0.635, 0.678, 1);

	// set the texture for the next draw command
	SetShaderTexture("wall");
	// UV scale (5.0, 5.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic wallpaper look
	SetTextureUVScale(5.0, 5.0);
	SetShaderMaterial("wall");

	// draw the mesh with transformation values
	// Drawing the wall
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/

						/* BASE */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the base
	scaleXYZ = glm::vec3(5.0f, 0.4f, 5.0f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the diamond orientation in the reference image
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Placing it in the middle of the room
	positionXYZ = glm::vec3(0.0f, 0.2f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Rose color base
	// SetShaderColor(0.8, 0.56, 0.631, 1);

	// set the texture for the next draw command
	SetShaderTexture("base");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic base of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("base");

	// draw the mesh with transformation values
	// Drawing the base
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/

					/* 1ST CYLINDER */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 1st pole
	scaleXYZ = glm::vec3(0.3f, 10.0f, 0.3f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the left back corner of the base
	positionXYZ = glm::vec3(-2.5f, 0.4f, -0.35f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Cream color pole
	//SetShaderColor(1, 0.874, 0.764, 1);

	// set the texture for the next draw command
	SetShaderTexture("pole");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic scratching post of a cat tree
	SetTextureUVScale(1.0, 15.0);
	SetShaderMaterial("pole");
	

	// draw the mesh with transformation values
	// Drawing the 1st pole
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

						/* 2nd CYLINDER */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 2nd pole
	scaleXYZ = glm::vec3(0.3f, 4.0f, 0.3f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the left front corner of the base
	positionXYZ = glm::vec3(-0.5f, 0.4f, 2.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Cream color pole
	// SetShaderColor(1, 0.874, 0.764, 1);
	
	// set the texture for the next draw command
	SetShaderTexture("pole");
	// UV scale (1.0, 16.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic scratching post of a cat tree
	SetTextureUVScale(1.0, 10.0);
	SetShaderMaterial("pole");

	// draw the mesh with transformation values
	// Drawing the 2nd pole
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

						/* 3rd CYLINDER */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 3rd pole
	scaleXYZ = glm::vec3(0.3f, 7.0f, 0.3f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the right back corner of the base
	positionXYZ = glm::vec3(0.5f, 0.4f, -2.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Cream color pole
	// SetShaderColor(1, 0.874, 0.764, 1);

	// set the texture for the next draw command
	SetShaderTexture("pole");
	// UV scale (1.0, 10.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic scratching post of a cat tree
	SetTextureUVScale(1.0, 10.0);
	SetShaderMaterial("pole");

	// draw the mesh with transformation values
	// Drawing the 3rd pole
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/


						/* 4th CYLINDER */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 4th pole
	scaleXYZ = glm::vec3(0.3f, 7.0f, 0.3f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning on the right front corner base
	positionXYZ = glm::vec3(2.5f, 0.4f, 0.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Cream color pole
	// SetShaderColor(1, 0.874, 0.764, 1);

	// set the texture for the next draw command
	SetShaderTexture("pole");
	// UV scale (1.0, 10.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic scratching post of a cat tree
	SetTextureUVScale(1.0, 10.0);
	SetShaderMaterial("pole");

	// draw the mesh with transformation values
	// Drawing the 4th pole
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

							/* 1st LANDING */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 1st landing
	scaleXYZ = glm::vec3(5.0f, 0.2f, 2.0f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the diamond orientation in the reference image
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning between the left and right back poles
	positionXYZ = glm::vec3(-1.1f, 3.0f, -1.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Light tan color landing
	// SetShaderColor(1, 0.792, 0.525, 1);

	// set the texture for the next draw command
	SetShaderTexture("wood");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic wood landing of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("wood");

	// draw the mesh with transformation values
	// Drawing the 1st landing
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


						/* 2nd LANDING */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 2nd landing
	scaleXYZ = glm::vec3(5.0f, 0.2f, 2.0f);

	// set the XYZ rotation for the 
	// Rotated 35 degrees to match the diamond orientation in the reference image
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning between the left and right front poles
	positionXYZ = glm::vec3(1.2f, 4.5f, 1.6f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Light tan color landing
	// SetShaderColor(1, 0.792, 0.525, 1);

	// set the texture for the next draw command
	SetShaderTexture("wood");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic wood landing of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("wood");

	// draw the mesh with transformation values
	// Drawing the 2nd landing
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


						/* 3rd LANDING */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 3rd landing
	scaleXYZ = glm::vec3(5.0f, 0.2f, 2.0f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the diamond orientation in the reference image
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning between the left and right back poles
	positionXYZ = glm::vec3(-0.7f, 7.5f, -0.9f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Rose color landing
	// SetShaderColor(0.8, 0.56, 0.631, 1);

	// set the texture for the next draw command
	SetShaderTexture("pink");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic furry landing of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("pink");

	// draw the mesh with transformation values
	// Drawing the 3rd landing
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/

							/* 4th LANDING */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 4th landing
	scaleXYZ = glm::vec3(3.0f, 0.2f, 2.0f);

	// set the XYZ rotation for the mesh
	// Rotating -55 degrees from the 35 degrees 3rd landing
	XrotationDegrees = 0.0f;
	YrotationDegrees = -55.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning on the top right pole and 90 degrees from the 3rd landing to create a "L" landing
	positionXYZ = glm::vec3(1.76f, 7.5f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Rose color landing
	// SetShaderColor(0.8, 0.56, 0.631, 1);

	// set the texture for the next draw command
	SetShaderTexture("pink");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic furry landing of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("pink");

	// draw the mesh with transformation values
	// Drawing the 4th landing
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


							/* BOX */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the enclosure
	scaleXYZ = glm::vec3(2.0f, 2.0f, 2.0f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the diamond orientation in the reference image
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning on top of the 3rd landing to the right 
	positionXYZ = glm::vec3(0.48f, 8.5f, -1.79f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Rose color enclosure
	// SetShaderColor(0.8, 0.56, 0.631, 1);
	
	// set the texture for the next draw command
	SetShaderTexture("pink");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic cubby enclosure of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("pink");

	// draw the mesh with transformation values
	// Drawing enclosure
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


						/* 1st PERCH */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the first perch
	scaleXYZ = glm::vec3(2.0f, 0.5f, 2.0f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the diamond orientation in the reference image
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the middle of the top of the left back pole
	positionXYZ = glm::vec3(-2.4f, 10.5f, -0.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Rose color perch
	// SetShaderColor(0.8, 0.56, 0.631, 1);

	// set the texture for the next draw command
	SetShaderTexture("pink");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic perch of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("pink");

	// draw the mesh with transformation values
	// Drawing the 1st perch
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


						/* 5th CYLINDER */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of 5th pole
	scaleXYZ = glm::vec3(0.3f, 2.5f, 0.3f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the middle and on top of enclosure
	positionXYZ = glm::vec3(0.5f, 9.5f, -1.7f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Cream color pole
	// SetShaderColor(1, 0.874, 0.764, 1);

	// set the texture for the next draw command
	SetShaderTexture("pole");
	// UV scale (1.0, 10.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic scratching post of a cat tree
	SetTextureUVScale(1.0, 10.0);
	SetShaderMaterial("pole");

	// draw the mesh with transformation values
	// Drawing the 5th pole
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

							/* 2nd PERCH */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 2nd perch
	scaleXYZ = glm::vec3(2.0f, 0.5f, 2.0f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the diamond orientation in the reference image
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the middle and on top of the 5th pole
	positionXYZ = glm::vec3(0.5f, 12.0f, -1.7f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Rose color perch
	// SetShaderColor(0.8, 0.56, 0.631, 1);

	// set the texture for the next draw command
	SetShaderTexture("pink");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic perch of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("pink");

	// draw the mesh with transformation values
	// Drawing the 2nd perch
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


							/* LADDER */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the ladder
	scaleXYZ = glm::vec3(1.5f, 0.2f, 3.5f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the diamond orientation in the reference image and 55 degrees to match the leaning orientation from 2nd landing to 3rd landing
	XrotationDegrees = 55.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the middle and on top of the 2nd landing
	positionXYZ = glm::vec3(-0.5f, 6.0f, 1.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Light tan color ladder
	// SetShaderColor(1, 0.792, 0.525, 1);

	// set the texture for the next draw command
	SetShaderTexture("wood");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic wood ladder of a cat tree
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("ladder");

	// draw the mesh with transformation values
	// Drawing the ladder
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


						/* 1st STRING */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the first string
	scaleXYZ = glm::vec3(0.02f, 1.5f, 0.02f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning on bottom of the left, front corner of the 1st perch
	positionXYZ = glm::vec3(-2.6f, 9.5f, 0.7f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// White color string
	// SetShaderColor(1, 1, 1, 1);

	// set the texture for the next draw command
	SetShaderTexture("holder");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic string holder of the ball toy
	SetTextureUVScale(1.0, 1.0);

	// draw the mesh with transformation values
	// Drawing the 1st string
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


						/* 1st BALL */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 1st ball
	scaleXYZ = glm::vec3(0.3f, 0.3f, 0.3f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning on the bottom of the 1st string
	positionXYZ = glm::vec3(-2.6f, 8.5f, 0.7f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// White color ball
	// SetShaderColor(1, 1, 1, 1);

	// set the texture for the next draw command
	SetShaderTexture("fur");
	// UV scale (5.0, 5.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic cat ball you
	SetTextureUVScale(5.0, 5.0);

	// draw the mesh with transformation values
	// Drawing the 1st ball
	m_basicMeshes->DrawSphereMesh();
	/****************************************************************/


							/* 2nd STRING */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 2nd string
	scaleXYZ = glm::vec3(0.02f, 1.5f, 0.02f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning on the bottom of the right,front corner of the 2nd perch
	positionXYZ = glm::vec3(1.5f, 10.5f, -1.5);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// White color string
	// SetShaderColor(1, 1, 1, 1);

	// set the texture for the next draw command
	SetShaderTexture("holder");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic string holder of the ball toy
	SetTextureUVScale(1.0, 1.0);

	// draw the mesh with transformation values
	// Drawing the 2nd string
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

							/* 2nd BALL */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the 2nd ball
	scaleXYZ = glm::vec3(0.3f, 0.3f, 0.3f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning on the bottom of the 2nd string
	positionXYZ = glm::vec3(1.5f, 10.2f, -1.5);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// White color ball
	// SetShaderColor(1, 1, 1, 1);

	// set the texture for the next draw command
	SetShaderTexture("fur");
	// UV scale (5.0, 5.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic cat ball you
	SetTextureUVScale(5.0, 5.0);

	// draw the mesh with transformation values
	// Drawing the 2nd ball
	m_basicMeshes->DrawSphereMesh();
	/****************************************************************/


						/* FRONT ENTRACE */
						/* HALF SPHERE */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the top of the archway
	scaleXYZ = glm::vec3(0.5f, 0.5f, 0.0f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the enclosure
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning the front of the enclosure
	positionXYZ = glm::vec3(1.1f, 8.6f, -1.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Gray color entrance
	// SetShaderColor(0.309, 0.309, 0.309, 1);

	// set the texture for the next draw command
	SetShaderTexture("top");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic entrance of a cat cubby
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("top");

	// draw the mesh with transformation values
	// Drawing the top of the entrance
	m_basicMeshes->DrawHalfSphereMesh();
	/****************************************************************/


							/* FRONT ENTRACE */
							/* CLYINDER */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the bottom of the archway
	scaleXYZ = glm::vec3(0.5f, 1.2f, 0.0f);

	// set the XYZ rotation for the mesh
	// Rotated 35 degrees to match the enclosure
	XrotationDegrees = 0.0f;
	YrotationDegrees = 35.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning the front of the enclosure and under the half sphere
	positionXYZ = glm::vec3(1.1f, 7.4f, -1.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Gray color entrance
	// SetShaderColor(0.309, 0.309, 0.309, 1);

	// set the texture for the next draw command
	SetShaderTexture("bottom");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic entrance of a cat cubby
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("bottom");


	// draw the mesh with transformation values
	// Drawing the bottom of the entrance
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/


							/* SIDE ENTRACE */
							/* HALF SPHERE */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the top of the archway
	scaleXYZ = glm::vec3(0.5f, 0.5f, 0.0f);

	// set the XYZ rotation for the mesh
	// Rotating -55 degrees to match the side of the enclosure
	XrotationDegrees = 0.0f;
	YrotationDegrees = -55.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning the side of the enclosure
	positionXYZ = glm::vec3(-0.33f, 8.6f, -1.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Gray color entrance
	// SetShaderColor(0.309, 0.309, 0.309, 1);

	// set the texture for the next draw command
	SetShaderTexture("top");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic entrance of a cat cubby
	SetTextureUVScale(1.0, 1.0);
	// Shader mat
	SetShaderMaterial("top");

	// draw the mesh with transformation values
	// Drawing the top of the entrance
	m_basicMeshes->DrawHalfSphereMesh();
	/****************************************************************/


							/* SIDE ENTRACE */
							/* CLYINDER */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the bottom of the archway
	scaleXYZ = glm::vec3(0.5f, 1.2f, 0.0f);

	// set the XYZ rotation for the mesh
	// Rotating -55 degrees to match the side of the enclosure
	XrotationDegrees = 0.0f;
	YrotationDegrees = -55.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning the side of the enclosure and under the half sphere
	positionXYZ = glm::vec3(-0.33f, 7.4f, -1.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Gray color entrance
	// SetShaderColor(0.309, 0.309, 0.309, 1);

	// set the texture for the next draw command
	SetShaderTexture("bottom");
	// UV scale (1.0, 1.0) to avoid stretch image and have a seamless look
	// Also, gives it a realistic entrance of a cat cubby
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("bottom");

	// draw the mesh with transformation values
	// Drawing the bottom of the entrance
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

							/* CAT BODY */
/*** Set needed transformations before drawing the basic mesh.  ***/
/*** This same ordering of code should be used for transforming ***/
/*** and drawing all the basic 3D shapes.						***/
/******************************************************************/
// set the XYZ scale for the mesh
// The size of the first perch
	scaleXYZ = glm::vec3(1.0f, 0.5f, 1.0f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the middle of the top of the left back pole
	positionXYZ = glm::vec3(-2.4f, 11.0f, -0.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	
	// Cat fur texture
	SetShaderTexture("cat");
	SetTextureUVScale(1.0, 1.0);

	// draw the mesh with transformation values
	// Drawing the cat body
	m_basicMeshes->DrawSphereMesh();
	/****************************************************************/

								/* CAT HEAD */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the first perch
	scaleXYZ = glm::vec3(0.5f, 0.25f, 0.5f);

	// set the XYZ rotation for the mesh
	// No rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning in the middle of the top of the left back pole
	positionXYZ = glm::vec3(-2.8f, 11.5f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	
	// Cat fur texture
	SetShaderTexture("cat");
	SetTextureUVScale(1.0, 1.0);

	// draw the mesh with transformation values
	// Drawing the cat head
	m_basicMeshes->DrawSphereMesh();
	/****************************************************************/

						/* CAT LEFT EAR */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the LEFT cat ear
	scaleXYZ = glm::vec3(0.1f, 0.22f, 0.05f);

	// set the XYZ rotation for the mesh
	// Ear to have a forward title and outward flare
	XrotationDegrees = 5.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 32.0f;

	// set the XYZ position for the mesh
	// Positioning the left ear on the left side of cat's head
	positionXYZ = glm::vec3(-3.05f, 11.7f, 0.1f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	
	
	// Cat fur texture
	SetShaderTexture("cat");
	SetTextureUVScale(1.0, 1.0);

	// draw the mesh with transformation values
	// Drawing the left ear
	m_basicMeshes->DrawConeMesh();
	/****************************************************************/


							/* CAT RIGHT EAR */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the right cat ear
	scaleXYZ = glm::vec3(0.1f, 0.22f, 0.05f);

	// set the XYZ rotation for the mesh
	// Ear to have a forward title and outward flare
	XrotationDegrees = 5.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -32.0f;

	// set the XYZ position for the mesh
	// Positioning the right ear on the right side of cat's head
	positionXYZ = glm::vec3(-2.55f, 11.7f, -0.1f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Cat fur texture
	SetShaderTexture("cat");
	SetTextureUVScale(1.0, 1.0);

	// draw the mesh with transformation values
	// Drawing the right ear
	m_basicMeshes->DrawConeMesh();
	/****************************************************************/


						/* BOTTOM OF THE BOWL */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the bottom of the cat bowl
	scaleXYZ = glm::vec3(0.4f, 0.5f, 0.4f);

	// set the XYZ rotation for the mesh
	// no rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning to the right of the cat tree 
	positionXYZ = glm::vec3(6.0f, 0.0f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Bowl texture
	SetShaderTexture("bowl");
	SetTextureUVScale(1.0, 1.0);

	// draw the mesh with transformation values
	// Drawing the bottom of the bowl
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/


						/* TOP OF THE BOWL */
	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	// The size of the top of the cat bowl
	scaleXYZ = glm::vec3(0.5f, 0.5f, 0.5f);

	// set the XYZ rotation for the mesh
	// no rotation needed
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	// Positioning on top of the bottom of the bowl
	positionXYZ = glm::vec3(6.0f, 0.5f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Bowl texture
	SetShaderTexture("bowl");
	SetTextureUVScale(1.0, 1.0);

	// draw the mesh with transformation values
	// Drawing the top of the bowl
	m_basicMeshes->DrawTorusMesh();
	/****************************************************************/
}
