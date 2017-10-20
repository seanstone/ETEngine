#pragma once

class ShaderData;
class Planet;

struct PatchVertex
{
	PatchVertex(glm::vec2 position, glm::vec2 morphVec)
	{
		pos = position;
		morph = morphVec;
	}
	glm::vec2 pos;
	glm::vec2 morph;
};
struct PatchInstance
{
	PatchInstance(BYTE Level, glm::vec3 A, glm::vec3 R, glm::vec3 S)
	{
		level = Level;
		a = A;
		r = R;
		s = S;
	}
	int32 level;
	glm::vec3 a;
	glm::vec3 r;
	glm::vec3 s;
};
class Patch
{
public:
	Patch(int16 levels = 5);
	~Patch();

	void SetPlanet(Planet* pPlanet) { m_pPlanet = pPlanet; }

	uint32 GetVertexCount() { return m_Vertices.size(); }

	void Init();
	void GenerateGeometry(int16 levels);
	void BindInstances(std::vector<PatchInstance> &instances);
	void UploadDistanceLUT(std::vector<float> &distances);
	void Draw(bool white = false);
private:
	std::vector<PatchVertex>m_Vertices;
	std::vector<uint32>m_Indices;

	Planet *m_pPlanet = nullptr;

	int32 m_NumInstances = 0;

	int16 m_Levels;
	uint32 m_RC;

	//OpenGl stuff
	GLuint m_VAO;
	GLuint m_VBO;
	GLuint m_EBO;
	GLuint m_VBOInstance;

	ShaderData *m_pPatchShader = nullptr;

	GLint m_uModel;
	GLint m_uViewProj;

	GLint m_uMaxHeight;

	GLint m_uCamPos;
	GLint m_uRadius;
	float m_MorphRange = 0.5f;
	GLint m_uMorphRange;

	GLint m_uDelta;

	//shading
	glm::vec3 m_Ambient = glm::vec3(0.05f, 0.05f, 0.08f);
	GLint m_uAmbient;
};