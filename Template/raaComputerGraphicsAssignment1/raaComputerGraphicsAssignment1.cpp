#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <gl/glut.h>
#include <map>
#include <conio.h>

#include <raaCamera/raaCamera.h>
#include <raaUtilities/raaUtilities.h>
#include <raaMaths/raaMaths.h>
#include <raaMaths/raaVector.h>
#include <raaSystem/raaSystem.h>
#include <raaPajParser/raaPajParser.h>
#include <raaText/raaText.h>

#include "raaConstants.h"
#include "raaParse.h"
#include "raaControl.h"

// NOTES
// look should look through the libraries and additional files I have provided to familarise yourselves with the functionallity and code.
// The data is loaded into the data structure, managed by the linked list library, and defined in the raaSystem library.
// You will need to expand the definitions for raaNode and raaArc in the raaSystem library to include additional attributes for the siumulation process
// If you wish to implement the mouse selection part of the assignment you may find the camProject and camUnProject functions usefull


// core system global data
raaCameraInput g_Input; // structure to hadle input to the camera comming from mouse/keyboard events
raaCamera g_Camera; // structure holding the camera position and orientation attributes
raaSystem g_System; // data structure holding the imported graph of data - you may need to modify and extend this to support your functionallity
raaControl g_Control; // set of flag controls used in my implmentation to retain state of key actions

// global var: parameter name for the file to load
const static char csg_acFileParam[] = {"-input"};

// global var: file to load data from
char g_acFile[256];

// core functions -> reduce to just the ones needed by glut as pointers to functions to fulfill tasks
void display(); // The rendering function. This is called once for each frame and you should put rendering code here
void idle(); // The idle function is called at least once per frame and is where all simulation and operational code should be placed
void reshape(int iWidth, int iHeight); // called each time the window is moved or resived
void keyboard(unsigned char c, int iXPos, int iYPos); // called for each keyboard press with a standard ascii key
void keyboardUp(unsigned char c, int iXPos, int iYPos); // called for each keyboard release with a standard ascii key
void sKeyboard(int iC, int iXPos, int iYPos); // called for each keyboard press with a non ascii key (eg shift)
void sKeyboardUp(int iC, int iXPos, int iYPos); // called for each keyboard release with a non ascii key (eg shift)
void mouse(int iKey, int iEvent, int iXPos, int iYPos); // called for each mouse key event
void motion(int iXPos, int iYPos); // called for each mouse motion event

// Non glut functions
void myInit(); // the myinit function runs once, before rendering starts and should be used for setup
void nodeDisplay(raaNode *pNode); // callled by the display function to draw nodes
void arcDisplay(raaArc *pArc); // called by the display function to draw arcs
void buildGrid(); // build the grid display list - display list are a performance optimization
void setContinentNodeAttributes(raaNode *pNode); //abstracts switch statement for continent shape
void drawLabels(raaNode* pNode); //draws labels inside a mattrix

// UI menu functions
void createGlutMenu();
void menu(int item);

// UI menu variables
enum MENU_TYPE
{
	MENU_TOGGLE_GRID,
	MENU_TOGGLE_SOLVER,
	MENU_DEFAULT_LAYOUT,
	MENU_WORLD_SYSTEM_LAYOUT,
	MENU_RANDOM_LAYOUT,
	MENU_SPEED_UP,
	MENU_SLOW_DOWN
};
MENU_TYPE currentItem = MENU_TOGGLE_GRID;
static int menuId, submenuId;
int solverToggle = 0, gridToggle = 1;

// Position alteration functions
void copyWorldSystemToCurrentPosition(raaNode* pNode);
void copyDefaultToCurrentPosition(raaNode *pNode);
void setWorldSystemPosition();

// Spring primer functions
void springPrimer();
void resetResultantForce(raaNode *pNode);
void deriveForces(raaArc *pArc);
void deriveTranslation(raaNode *pNode);

// Spring primer variables
const float DAMPING_COEF = 0.99995f;
static float timeStep = 1.0f;

void springPrimer()
{
	if (solverToggle == 1)
	{
		// Step 1
		visitNodes(&g_System, resetResultantForce);

		// Step 2
		visitArcs(&g_System, deriveForces);

		// Step 3
		visitNodes(&g_System, deriveTranslation);
	}
}

void deriveForces(raaArc *pArc)
{
	raaNode *pNode_0 = pArc->m_pNode0;
	raaNode *pNode_1 = pArc->m_pNode1;

	// Resultant vector between 2 nodes and the magnitude of this vector
	float resultantVector[3];
	vecSub(pNode_1->m_afPosition, pNode_0->m_afPosition, resultantVector);
	long double distance = vecLength(resultantVector);

	// The unit vector derivation of the resultant vector
	float resultantUnitVector[3];
	for (int i = 0; i < 3; i++)
		resultantUnitVector[i] = resultantVector[i] / distance;

	// Extension through distance and base arc length and its 3D vector
	float extension = distance - pArc->m_fIdealLen;
	float extensionVector[3];
	vecScalarProduct(resultantUnitVector, extension, extensionVector);

	// Spring force vector = scalar product of extension vector with spring coefficient
	float springForce_0[3];
	vecScalarProduct(extensionVector, pArc->m_fSpringCoef, springForce_0);

	// Spring force vector in the opposite direction for the second node
	float springForce_1[3];
	vecScalarProduct(springForce_0, -1.0f, springForce_1);

	// Update resultant force
	vecAdd(pNode_0->m_resultantForce, springForce_0, pNode_0->m_resultantForce);
	vecAdd(pNode_1->m_resultantForce, springForce_1, pNode_1->m_resultantForce);
}

void deriveTranslation(raaNode *pNode)
{
	// Acceleration vector derived from force vector and mass
	float acceleration[3];
	for (int i = 0; i < 3; i++)
		acceleration[i] = pNode->m_resultantForce[i] / pNode->m_fMass;

	// Velocity vector for unit time = the sum of current velocity of the node and its acceleration, considering damping
	float velocity[3];
	for (int i = 0; i < 3; i++)
		velocity[i] = (pNode->m_velocity[i] + acceleration[i]) * timeStep * (1 - DAMPING_COEF);

	// New velocity set as current velocity for the node
	vecCopy(velocity, pNode->m_velocity);
	
	// Translation of the node is equal to the current velocity divided by time
	float displacement[3];
	for (int i = 0; i < 3; i++)
	{
		displacement[i] = pNode->m_velocity[i] / timeStep;
	}

	vecAdd(pNode->m_afPosition, displacement, pNode->m_afPosition);
}

void resetResultantForce(raaNode *pNode)
{
	vecInit(pNode->m_resultantForce);
}

void copyDefaultToCurrentPosition(raaNode *pNode)
{
	vecCopy(pNode->m_defaultPosition, pNode->m_afPosition);
}

void copyWorldSystemToCurrentPosition(raaNode* pNode)
{
	vecCopy(pNode->m_worldSystemPosition, pNode->m_afPosition);
}

void setWorldSystemPosition()
{
	int worldPosCount1 = 0, worldPosCount2 = 0, worldPosCount3 = 0;
	/* for each node in the nodes list,
	 * adjust x position based on world system unit,
	 * adjust y position based on previous node position,
	 */
	for (raaLinkedListElement *pE = g_System.m_llNodes.m_pHead; pE; pE = pE->m_pNext)
	{
		raaNode *pNode = (raaNode*)pE->m_pData;
		pNode->m_worldSystemPosition[0] = 300.0f * pNode->m_uiWorldSystem;
		
		if (pNode->m_uiWorldSystem == 1)
		{
			pNode->m_worldSystemPosition[1] = 50 * worldPosCount1;
			pNode->m_worldSystemPosition[2] = 100;
			worldPosCount1++;
		}
		else if (pNode->m_uiWorldSystem == 2)
		{
			pNode->m_worldSystemPosition[1] = 50 * worldPosCount2;
			pNode->m_worldSystemPosition[2] = 0;
			worldPosCount2++;
		}
		else if (pNode->m_uiWorldSystem == 3)
		{
			pNode->m_worldSystemPosition[1] = 50 * worldPosCount3;
			pNode->m_worldSystemPosition[2] = 100;
			worldPosCount3++;
		}
	}
}

void randomisePosition(raaNode* pNode)
{
	vecRand(100, 1000, pNode->m_afPosition);
}

void createGlutMenu()
{
	// Sub menu entries
	submenuId = glutCreateMenu(menu);
	glutAddMenuEntry("Default", MENU_DEFAULT_LAYOUT);
	glutAddMenuEntry("World System Layout", MENU_WORLD_SYSTEM_LAYOUT);
	glutAddMenuEntry("Randomised Layout", MENU_RANDOM_LAYOUT);

	// Main menu entries
	menuId = glutCreateMenu(menu);
	glutAddMenuEntry("Toggle Grid", MENU_TOGGLE_GRID);
	glutAddMenuEntry("Toggle Solver", MENU_TOGGLE_SOLVER);
	glutAddMenuEntry("Speed Up", MENU_SPEED_UP);
	glutAddMenuEntry("Slow Down", MENU_SLOW_DOWN);
	glutAddSubMenu("Switch Layouts", submenuId);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void menu(int item)
{
	switch (item)
	{
	case MENU_DEFAULT_LAYOUT:
	{
		visitNodes(&g_System, copyDefaultToCurrentPosition);
		solverToggle = 0;
		currentItem = (MENU_TYPE)item;
	}
	break;
	case MENU_WORLD_SYSTEM_LAYOUT:
	{
		visitNodes(&g_System, copyWorldSystemToCurrentPosition);
		solverToggle = 0;
		currentItem = (MENU_TYPE)item;
	}
	break;
	case MENU_RANDOM_LAYOUT:
	{
		visitNodes(&g_System, randomisePosition);
		solverToggle = 0;
		currentItem = (MENU_TYPE)item;
	}
	break;
	case MENU_TOGGLE_GRID:
	{
		if (gridToggle == 0)
			gridToggle = 1;
		else
			gridToggle = 0;
		currentItem = (MENU_TYPE)item;
	}
		break;
	case MENU_TOGGLE_SOLVER:
	{
		if (solverToggle == 0)
			solverToggle = 1;
		else
			solverToggle = 0;
		currentItem = (MENU_TYPE)item;
	}
		break;
	case MENU_SPEED_UP:
	{
		timeStep -= 0.1;
		currentItem = (MENU_TYPE)item;
	}
		break;
	case MENU_SLOW_DOWN:
	{
		timeStep += 0.1;
		currentItem = (MENU_TYPE)item;
	}
		break;
	default:
		break;
	}

	glutPostRedisplay();
}

void setContinentNodeAttributes(raaNode *pNode)
{
	int continent = pNode->m_uiContinent;
	int worldSystem = pNode->m_uiWorldSystem;
	float* position = pNode->m_afPosition;
	glTranslated(position[0], position[1], position[2]);
	switch (continent)
	{
		case 1: // Indigo
			utilitiesColourToMat(new float[4] { 0.29f, 0.0f, 0.51f, 1.0f }, 1.0f);
			break;
		case 2: // Gray
			utilitiesColourToMat(new float[4]{ 0.7f, 0.7f, 0.7f, 1.0f }, 1.0f);
			break;
		case 3: // Blue
			utilitiesColourToMat(new float[4] { 0.0f, 0.0f, 1.0f, 1.0f }, 1.0f);
			break;
		case 4: // Red
			utilitiesColourToMat(new float[4] { 1.0f, 0.0f, 0.0f, 1.0f }, 1.0f);
			break;
		case 5: // Green
			utilitiesColourToMat(new float[4] { 0.0f, 1.0f, 0.0f, 1.0f }, 1.0f);
			break;
		case 6: // Orange
			utilitiesColourToMat(new float[4] { 1.0f, 0.5f, 0.0f, 1.0f }, 1.0f);
			break;			
	}
	switch (worldSystem)
	{
	case 1: // Cube
		glutSolidCube(mathsDimensionOfCubeFromVolume(pNode->m_fMass));
		break;
	case 2: // Sphere
		glutSolidSphere(mathsRadiusOfSphereFromVolume(pNode->m_fMass), 15, 15);
		break;
	case 3: // Doughnut
		glutSolidTorus(5.0f, mathsRadiusOfSphereFromVolume(pNode->m_fMass), 15, 15);
		break;
	}
	glMultMatrixf(camRotMatInv(g_Camera));
	drawLabels(pNode);
}

void drawLabels(raaNode* pNode)
{
	glScalef(16.0f, 16.0f, 0.1f);
	glTranslatef(0.0f, 1.50f, 0.0f);
	outlinePrint(pNode->m_acName);
}

void nodeDisplay(raaNode *pNode) // function to render a node (called from display())
{
	glPushMatrix();
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	
	setContinentNodeAttributes(pNode);

	glPopAttrib();
	glPopMatrix();
}

void arcDisplay(raaArc *pArc) // function to render an arc (called from display())
{
	// put your arc rendering (ogl) code here

	raaNode* node0 = pArc->m_pNode0;
	raaNode* node1 = pArc->m_pNode1;

	glEnable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
	
	glBegin(GL_LINES);

		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex3f(node0->m_afPosition[0], node0->m_afPosition[1], node0->m_afPosition[2]);
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex3f(node1->m_afPosition[0], node1->m_afPosition[1], node1->m_afPosition[2]);
	
	glEnd();
}

// draw the scene. Called once per frame and should only deal with scene drawing (not updating the simulator)
void display() 
{
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT); // clear the rendering buffers

	glLoadIdentity(); // clear the current transformation state
	glMultMatrixf(camObjMat(g_Camera)); // apply the current camera transform

	// draw the grid if the control flag for it is true	
	if (gridToggle == 1) glCallList(gs_uiGridDisplayList);

	glPushAttrib(GL_ALL_ATTRIB_BITS); // push attribute state to enable constrained state changes
	visitNodes(&g_System, nodeDisplay); // loop through all of the nodes and draw them with the nodeDisplay function
	visitArcs(&g_System, arcDisplay); // loop through all of the arcs and draw them with the arcDisplay function
	glPopAttrib();


	// draw a simple sphere
	float afCol[] = { 0.3f, 1.0f, 0.5f, 1.0f };
	utilitiesColourToMat(afCol, 2.0f);

	glPushMatrix();
	glTranslatef(0.0f, 30.0f, 0.0f);
	glutSolidSphere(5.0f, 10, 10);
	glPopMatrix();

	glFlush(); // ensure all the ogl instructions have been processed
	glutSwapBuffers(); // present the rendered scene to the screen
}

// processing of system and camera data outside of the renderng loop
void idle() 
{
	controlChangeResetAll(g_Control); // re-set the update status for all of the control flags
	camProcessInput(g_Input, g_Camera); // update the camera pos/ori based on changes since last render
	camResetViewportChanged(g_Camera); // re-set the camera's viwport changed flag after all events have been processed
	springPrimer(); // all spring based simulation functionality updating node position
	glutPostRedisplay();// ask glut to update the screen
}

// respond to a change in window position or shape
void reshape(int iWidth, int iHeight)  
{
	glViewport(0, 0, iWidth, iHeight);  // re-size the rendering context to match window
	camSetViewport(g_Camera, 0, 0, iWidth, iHeight); // inform the camera of the new rendering context size
	glMatrixMode(GL_PROJECTION); // switch to the projection matrix stack 
	glLoadIdentity(); // clear the current projection matrix state
	gluPerspective(csg_fCameraViewAngle, ((float)iWidth)/((float)iHeight), csg_fNearClip, csg_fFarClip); // apply new state based on re-sized window
	glMatrixMode(GL_MODELVIEW); // swap back to the model view matrix stac
	glGetFloatv(GL_PROJECTION_MATRIX, g_Camera.m_afProjMat); // get the current projection matrix and sort in the camera model
	glutPostRedisplay(); // ask glut to update the screen
}

// detect key presses and assign them to actions
void keyboard(unsigned char c, int iXPos, int iYPos)
{
	switch(c)
	{
	case 'w':
		camInputTravel(g_Input, tri_pos); // mouse zoom
		break;
	case 's':
		camInputTravel(g_Input, tri_neg); // mouse zoom
		break;
	case 'c':
		camPrint(g_Camera); // print the camera data to the comsole
		break;
	case 'g':
		controlToggle(g_Control, csg_uiControlDrawGrid); // toggle the drawing of the grid
		break;
	}
}

// detect standard key releases
void keyboardUp(unsigned char c, int iXPos, int iYPos) 
{
	switch(c)
	{
		// end the camera zoom action
		case 'w': 
		case 's':
			camInputTravel(g_Input, tri_null);
			break;
	}
}

void sKeyboard(int iC, int iXPos, int iYPos)
{
	// detect the pressing of arrow keys for ouse zoom and record the state for processing by the camera
	switch(iC)
	{
		case GLUT_KEY_UP:
			camInputTravel(g_Input, tri_pos);
			break;
		case GLUT_KEY_DOWN:
			camInputTravel(g_Input, tri_neg);
			break;
	}
}

void sKeyboardUp(int iC, int iXPos, int iYPos)
{
	// detect when mouse zoom action (arrow keys) has ended
	switch(iC)
	{
		case GLUT_KEY_UP:
		case GLUT_KEY_DOWN:
			camInputTravel(g_Input, tri_null);
			break;
	}
}

void mouse(int iKey, int iEvent, int iXPos, int iYPos)
{
	// capture the mouse events for the camera motion and record in the current mouse input state
	if (iKey == GLUT_LEFT_BUTTON)
	{
		camInputMouse(g_Input, (iEvent == GLUT_DOWN) ? true : false);
		if (iEvent == GLUT_DOWN)camInputSetMouseStart(g_Input, iXPos, iYPos);
	}
	else if (iKey == GLUT_MIDDLE_BUTTON)
	{
		camInputMousePan(g_Input, (iEvent == GLUT_DOWN) ? true : false);
		if (iEvent == GLUT_DOWN)camInputSetMouseStart(g_Input, iXPos, iYPos);
	}
}

void motion(int iXPos, int iYPos)
{
	// if mouse is in a mode that tracks motion pass this to the camera model
	if(g_Input.m_bMouse || g_Input.m_bMousePan) camInputSetMouseLast(g_Input, iXPos, iYPos);
}


void myInit()
{
	// setup my event control structure
	controlInit(g_Control);

	// initalise the maths library
	initMaths();

	// Camera setup
	camInit(g_Camera); // initalise the camera model
	camInputInit(g_Input); // initialise the persistant camera input data 
	camInputExplore(g_Input, true); // define the camera navigation mode

	// opengl setup - this is a basic default for all rendering in the render loop
	glClearColor(csg_afColourClear[0], csg_afColourClear[1], csg_afColourClear[2], csg_afColourClear[3]); // set the window background colour
	glEnable(GL_DEPTH_TEST); // enables occusion of rendered primatives in the window
	glEnable(GL_LIGHT0); // switch on the primary light
	glEnable(GL_LIGHTING); // enable lighting calculations to take place
	glEnable(GL_BLEND); // allows transparency and fine lines to be drawn
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // defines a basic transparency blending mode
	glEnable(GL_NORMALIZE); // normalises the normal vectors used for lighting - you may be able to switch this iff (performance gain) is you normalise all normals your self
	glEnable(GL_CULL_FACE); // switch on culling of unseen faces
	glCullFace(GL_BACK); // set culling to not draw the backfaces of primatives

	// build the grid display list - display list are a performance optimization 
	buildGrid();

	// initialise the data system and load the data file
	initSystem(&g_System);
	parse(g_acFile, parseSection, parseNetwork, parseArc, parsePartition, parseVector);
	setWorldSystemPosition(); // sets world position on all nodes
}

int main(int argc, char* argv[])
{
	// check parameters to pull out the path and file name for the data file
	for (int i = 0; i<argc; i++) if (!strcmp(argv[i], csg_acFileParam)) sprintf_s(g_acFile, "%s", argv[++i]);


	if (strlen(g_acFile)) 
	{ 
		// if there is a data file

		glutInit(&argc, (char**)argv); // start glut (opengl window and rendering manager)

		glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA); // define buffers to use in ogl
		glutInitWindowPosition(csg_uiWindowDefinition[csg_uiX], csg_uiWindowDefinition[csg_uiY]);  // set rendering window position
		glutInitWindowSize(csg_uiWindowDefinition[csg_uiWidth], csg_uiWindowDefinition[csg_uiHeight]); // set rendering window size
		glutCreateWindow("raaAssignment1-2017");  // create rendering window and give it a name

		createGlutMenu();

		buildFont(); // setup text rendering (use outline print function to render 3D text
		
		myInit(); // application specific initialisation

		// provide glut with callback functions to enact tasks within the event loop
		glutDisplayFunc(display);
		glutIdleFunc(idle);
		glutReshapeFunc(reshape);
		glutKeyboardFunc(keyboard);
		glutKeyboardUpFunc(keyboardUp);
		glutSpecialFunc(sKeyboard);
		glutSpecialUpFunc(sKeyboardUp);
		glutMouseFunc(mouse);
		glutMotionFunc(motion);
		glutMainLoop(); // start the rendering loop running, this will only ext when the rendering window is closed 

		killFont(); // cleanup the text rendering process

		return 0; // return a null error code to show everything worked
	}
	else
	{
		// if there isn't a data file 

		printf("The data file cannot be found, press any key to exit...\n");
		_getch();
		return 1; // error code
	}
}

void buildGrid()
{
	if (!gs_uiGridDisplayList) gs_uiGridDisplayList= glGenLists(1); // create a display list

	glNewList(gs_uiGridDisplayList, GL_COMPILE); // start recording display list

	glPushAttrib(GL_ALL_ATTRIB_BITS); // push attrib marker
	glDisable(GL_LIGHTING); // switch of lighting to render lines

	glColor4fv(csg_afDisplayListGridColour); // set line colour

	// draw the grid lines
	glBegin(GL_LINES);
	for (int i = (int)csg_fDisplayListGridMin; i <= (int)csg_fDisplayListGridMax; i++)
	{
		glVertex3f(((float)i)*csg_fDisplayListGridSpace, 0.0f, csg_fDisplayListGridMin*csg_fDisplayListGridSpace);
		glVertex3f(((float)i)*csg_fDisplayListGridSpace, 0.0f, csg_fDisplayListGridMax*csg_fDisplayListGridSpace);
		glVertex3f(csg_fDisplayListGridMin*csg_fDisplayListGridSpace, 0.0f, ((float)i)*csg_fDisplayListGridSpace);
		glVertex3f(csg_fDisplayListGridMax*csg_fDisplayListGridSpace, 0.0f, ((float)i)*csg_fDisplayListGridSpace);
	}
	glEnd(); // end line drawing

	glPopAttrib(); // pop attrib marker (undo switching off lighting)

	glEndList(); // finish recording the displaylist
}
