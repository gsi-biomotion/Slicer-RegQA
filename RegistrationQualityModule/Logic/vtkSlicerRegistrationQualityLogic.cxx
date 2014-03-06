#include "vtkSlicerRegistrationQualityLogic.h"

// RegistrationQuality includes
#include "vtkMRMLRegistrationQualityNode.h"
#include "vtkDFVGlyph3D.h"

// SlicerQt Includes
#include "qSlicerApplication.h"
#include "qSlicerCLILoadableModuleFactory.h"
#include "qSlicerCLIModule.h"
#include "qSlicerCLIModuleWidget.h"
#include "qSlicerModuleFactoryManager.h"
#include "qSlicerModuleManager.h"
#include <vtkSlicerCLIModuleLogic.h>

// MRML includes
#include <vtkMRMLVectorVolumeNode.h>
#include <vtkMRMLModelDisplayNode.h>
#include <vtkMRMLScalarVolumeDisplayNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include "vtkSlicerVolumesLogic.h"
#include "vtkMRMLViewNode.h"
#include "vtkSlicerCLIModuleLogic.h"
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLSliceLogic.h>

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkVectorNorm.h>
#include <vtkTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkGeneralTransform.h>
#include <vtkLookupTable.h>
#include <vtkMath.h>
#include <vtkImageAccumulate.h>


// Glyph VTK includes
#include <vtkArrowSource.h>
#include <vtkConeSource.h>
#include <vtkSphereSource.h>

// Grid VTK includes
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkPoints.h>
#include <vtkLine.h>
#include <vtkWarpVector.h>

// Block VTK includes
#include <vtkGeometryFilter.h>
#include <vtkVectorDot.h>
#include <vtkPolyDataNormals.h>

// Contour VTK includes
#include <vtkMarchingCubes.h>

// Glyph Slice VTK includes
#include <vtkGlyphSource2D.h>
#include <vtkRibbonFilter.h>

// Grid Slice VTK includes
#include <vtkAppendPolyData.h>

// STD includes
#include <iostream>
#include <cassert>
#include <math.h>

class vtkSlicerRegistrationQualityLogic::vtkInternal {
public:
	vtkInternal();
	vtkSlicerVolumesLogic* VolumesLogic;
};

//----------------------------------------------------------------------------
vtkSlicerRegistrationQualityLogic::vtkInternal::vtkInternal() {
	this->VolumesLogic = 0;
}
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerRegistrationQualityLogic);

//----------------------------------------------------------------------------
vtkSlicerRegistrationQualityLogic::vtkSlicerRegistrationQualityLogic() {
	this->RegistrationQualityNode = NULL;
	this->TransformField = vtkSmartPointer<vtkImageData>::New();
	this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerRegistrationQualityLogic::~vtkSlicerRegistrationQualityLogic() {
	vtkSetAndObserveMRMLNodeMacro(this->RegistrationQualityNode, NULL);
	delete this->Internal;
}
//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::SetVolumesLogic(vtkSlicerVolumesLogic* logic) {
	this->Internal->VolumesLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerVolumesLogic* vtkSlicerRegistrationQualityLogic::GetVolumesLogic() {
	return this->Internal->VolumesLogic;
}
//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::PrintSelf(ostream& os, vtkIndent indent) {
	this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic
::SetAndObserveRegistrationQualityNode(vtkMRMLRegistrationQualityNode *node) {
	vtkSetAndObserveMRMLNodeMacro(this->RegistrationQualityNode, node);
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene) {
	vtkNew<vtkIntArray> events;
	events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
	events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
	events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
	this->SetAndObserveMRMLSceneEvents(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::RegisterNodes() {
	vtkMRMLScene* scene = this->GetMRMLScene();
	assert(scene != 0);

	scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLRegistrationQualityNode>::New());
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::UpdateFromMRMLScene() {
	assert(this->GetMRMLScene() != 0);
	this->Modified();
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node) {
	if (!node || !this->GetMRMLScene()) {
		return;
	}

	if( node->IsA("vtkMRMLVectorVolumeNode") ||
		node->IsA("vtkMRMLLinearTransformNode") ||
		node->IsA("vtkMRMLGridTransformNode") ||
		node->IsA("vtkMRMLBSplineTransformNode") ||
		node->IsA("vtkMRMLRegistrationQualityNode")) {
		this->Modified();
	}
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::OnMRMLSceneNodeRemoved(vtkMRMLNode* node) {
	if (!node || !this->GetMRMLScene()) {
		return;
	}

	if (node->IsA("vtkMRMLVectorVolumeNode") ||
		node->IsA("vtkMRMLLinearTransformNode") ||
		node->IsA("vtkMRMLGridTransformNode") ||
		node->IsA("vtkMRMLBSplineTransformNode") ||
		node->IsA("vtkMRMLRegistrationQualityNode")) {
		this->Modified();
	}
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::OnMRMLSceneEndImport() {
	//Select parameter node if it exists
	vtkSmartPointer<vtkMRMLRegistrationQualityNode> paramNode = NULL;
	vtkSmartPointer<vtkMRMLNode> node = this->GetMRMLScene()->GetNthNodeByClass(
			0, "vtkMRMLRegistrationQualityNode");

	if (node) {
		paramNode = vtkMRMLRegistrationQualityNode::SafeDownCast(node);
		vtkSetAndObserveMRMLNodeMacro(this->RegistrationQualityNode, paramNode);
	}
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::OnMRMLSceneEndClose() {
	this->Modified();
}

void vtkSlicerRegistrationQualityLogic::SquaredDifference(int state) {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
	    vtkErrorMacro("SquaredDifference: Invalid scene or parameter set node!");
	    return;
	}

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));

	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));


// 	vtkMRMLScalarVolumeNode *outputVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetSquaredDiffVolumeNodeID()));
	if (!referenceVolume || !warpedVolume ) {
		vtkErrorMacro("SquaredDifference: Invalid reference or warped volume!");
		return;
	}

	if (!state) {
	      	this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}


	if (!this->RegistrationQualityNode->GetSquaredDiffVolumeNodeID()){
	  if(!this->Internal->VolumesLogic)
	      {
		std::cerr << "SquaredDifference: ERROR: failed to get hold of Volumes logic" << std::endl;
		return;
	      }

	  vtkMRMLScalarVolumeNode *outputVolume = NULL;
	    vtkMRMLScalarVolumeNode *svnode = vtkMRMLScalarVolumeNode::SafeDownCast(referenceVolume);
	    std::ostringstream outSS;

	    outSS << referenceVolume->GetName() << "-squaredDifference";
	    if(svnode)
	    {
	      outputVolume = this->Internal->VolumesLogic->CloneVolume(this->GetMRMLScene(), referenceVolume, outSS.str().c_str());
	    }
	    else
	    {
	      std::cerr << "Reference volume not scalar volume!" << std::endl;
	      return;
	    }

	  if ( !outputVolume ) {
		  vtkErrorMacro("SquaredDifference: No output volume set!");
		  return;
	  }
	  //Check dimensions of both volume, they must be the same.
	  vtkSmartPointer<vtkImageData> imageDataRef = referenceVolume->GetImageData();
	  vtkSmartPointer<vtkImageData> imageDataWarp = warpedVolume->GetImageData();
	    int* dimsRef = imageDataRef->GetDimensions();
	    int* dimsWarp = imageDataWarp->GetDimensions();
	  // int dims[3]; // can't do this
	  if (dimsRef[0] != dimsWarp[0] || dimsRef[1] != dimsWarp[1] || dimsRef[2] != dimsWarp[2] ) {
	    vtkErrorMacro("SquaredDifference: Dimensions of Reference and Warped image don't match'!");
	    return;
	  }

	  qSlicerCLIModule* checkerboardfilterCLI = dynamic_cast<qSlicerCLIModule*>(
			  qSlicerCoreApplication::application()->moduleManager()->module("SquaredDifference"));
	  QString cliModuleName("SquaredDifference");

	  vtkSmartPointer<vtkMRMLCommandLineModuleNode> cmdNode =
			  checkerboardfilterCLI->cliModuleLogic()->CreateNodeInScene();

	  // Set node parameters
	  cmdNode->SetParameterAsString("inputVolume1", referenceVolume->GetID());
	  cmdNode->SetParameterAsString("inputVolume2", warpedVolume->GetID());
	  cmdNode->SetParameterAsString("outputVolume", outputVolume->GetID());

	  // Execute synchronously so that we can check the content of the file after the module execution
	  checkerboardfilterCLI->cliModuleLogic()->ApplyAndWait(cmdNode);

	  this->GetMRMLScene()->RemoveNode(cmdNode);

	  outputVolume->SetAndObserveTransformNodeID(NULL);
	  this->RegistrationQualityNode->SetSquaredDiffVolumeNodeID(outputVolume->GetID());
	}


	vtkMRMLScalarVolumeNode *squaredDiffVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetSquaredDiffVolumeNodeID()));

	if ( !squaredDiffVolume ) {
		vtkErrorMacro("SquaredDifference: No output volume set!");
		return;
	}
	squaredDiffVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
	int window=300;
	int level=200;

	squaredDiffVolume->GetScalarVolumeDisplayNode()->SetThreshold(0,3e3);
	squaredDiffVolume->GetScalarVolumeDisplayNode()->SetLevel(level);
	squaredDiffVolume->GetScalarVolumeDisplayNode()->SetWindow(window);
	squaredDiffVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeRainbow");

	this->SetForegroundImage(referenceVolume,squaredDiffVolume,0.5);

	  // Get mean and std from squared difference volume
	double statisticValues[4];
	this->CalculateStatistics(squaredDiffVolume,statisticValues);

	this->RegistrationQualityNode->DisableModifiedEventOn();
	this->RegistrationQualityNode->SetSquaredDiffStatistics( statisticValues );
	this->RegistrationQualityNode->DisableModifiedEventOff();

	return;
}


//--- Image Checks -----------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::FalseColor(int state) {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("FalseColor: Invalid scene or parameter set node!");
		return;
	}
	std::cerr << "Setting false color." << std::endl;

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));

	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));

	if (!referenceVolume || !warpedVolume) {
		vtkErrorMacro("Falsecolor: Invalid reference or warped volume!");
		return;
	}
	//TODO: Volumes go back to gray value - perhaps we should rembemer previous color settings?
	if (!state) {
	      	this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}
	referenceVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeWarmTint1");
	warpedVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeCoolTint1");
	// Set window and level the same for warped and reference volume.
	warpedVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
	double window, level;
	window = referenceVolume->GetScalarVolumeDisplayNode()->GetWindow();
	level = referenceVolume->GetScalarVolumeDisplayNode()->GetLevel();

	warpedVolume->GetScalarVolumeDisplayNode()->SetWindow(window);
	warpedVolume->GetScalarVolumeDisplayNode()->SetLevel(level);

// 	// Set warped image in background
// 	qSlicerApplication *app = qSlicerApplication::application();
// 	qSlicerLayoutManager *layoutManager = app->layoutManager();
//
// 	if (!layoutManager) {
// 		return;
// 	}

	//TODO Bad Solution. Linking layers somehow doesn't work - it only changes one (red) slice.
// 	this->SetForegroundImage(vtkMRMLSliceCompositeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed")),referenceVolume,warpedVolume,0.5);
// 	this->SetForegroundImage(vtkMRMLSliceCompositeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow")),referenceVolume,warpedVolume,0.5);
	this->SetForegroundImage(referenceVolume,warpedVolume,0.5);

	return;
}

//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::Flicker(int opacity) {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Flicker: Invalid scene or parameter set node!");
		return;
	}

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));

	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));

	if (!referenceVolume || !warpedVolume) {
		vtkErrorMacro("Flicker: Invalid reference or warped volume!");
		return;
	}

// 	// Set warped image in background
// 	qSlicerApplication * app = qSlicerApplication::application();
// 	qSlicerLayoutManager * layoutManager = app->layoutManager();
//
// 	if (!layoutManager) {
// 		return;
// 	}
//
// 	vtkMRMLSliceCompositeNode *rcn = vtkMRMLSliceCompositeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
// 	vtkMRMLSliceCompositeNode *ycn = vtkMRMLSliceCompositeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
// 	vtkMRMLSliceCompositeNode *gcn = vtkMRMLSliceCompositeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
//
// 	vtkMRMLSliceLogic* redSliceLogic = rcn != NULL ?
// 			GetMRMLApplicationLogic()->GetSliceLogicByLayoutName(rcn->GetLayoutName()) : NULL;
// 	if (redSliceLogic == NULL) {
// 		vtkErrorMacro("Flicker: Invalid SliceLogic!");
// 		return;
// 	}
//
// 	// Link Slice Controls
// 	rcn->SetLinkedControl(1);
// 	ycn->SetLinkedControl(1);
// 	gcn->SetLinkedControl(1);
//
// 	// Set volumes and opacity for all three layers.
// 	redSliceLogic->StartSliceCompositeNodeInteraction(
// 		vtkMRMLSliceCompositeNode::ForegroundVolumeFlag
// 		| vtkMRMLSliceCompositeNode::BackgroundVolumeFlag);
// 	rcn->SetBackgroundVolumeID(referenceVolume->GetID());
// 	rcn->SetForegroundVolumeID(warpedVolume->GetID());
// 	rcn->SetForegroundOpacity(opacity);
// 	redSliceLogic->EndSliceCompositeNodeInteraction();
	this->SetForegroundImage(referenceVolume,warpedVolume,opacity);

	return;
}

void vtkSlicerRegistrationQualityLogic
::getSliceCompositeNodeRASBounds(vtkMRMLSliceCompositeNode *scn, double* minmax) {

	vtkMRMLScalarVolumeNode* foreground = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(scn->GetBackgroundVolumeID()));
	vtkMRMLScalarVolumeNode* background = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(scn->GetForegroundVolumeID()));

	double rasBounds[6] = {INFINITY,-INFINITY,INFINITY,-INFINITY,INFINITY,-INFINITY};
	double rasBoundsBack[6] = {INFINITY,-INFINITY,INFINITY,-INFINITY,INFINITY,-INFINITY};
	if(foreground) foreground->GetRASBounds(rasBounds);
	if(background) background->GetRASBounds(rasBoundsBack);
	for(int i=0;i<3; i++) {
		minmax[2*i] = std::min(rasBounds[2*i],rasBoundsBack[2*i]);
		minmax[2*i+1] = std::max(rasBounds[2*i+1],rasBoundsBack[2*i+1]);
		if(minmax[2*i]>minmax[2*i+1]) {
			cout << "rasBounds infty" << endl;
			minmax[2*i] = minmax[2*i+1] = 0;
		}
	}
}

//----------------------------------------------------------------------------
/**
 * Movie through slices.
 * TODO:	- Calculate slice spacing
 * 			- Changed slice directions
 * 			- Oblique slices
 */
void vtkSlicerRegistrationQualityLogic::Movie() {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Movie: Invalid scene or parameter set node!");
		return;
	}
	vtkMRMLSliceNode* sliceNodeRed = vtkMRMLSliceNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
	vtkMRMLSliceNode* sliceNodeYellow = vtkMRMLSliceNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
	vtkMRMLSliceNode* sliceNodeGreen = vtkMRMLSliceNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));

	double rasBoundsRed[6], rasBoundsYellow[6], rasBoundsGreen[6];
	int runState = this->RegistrationQualityNode->GetMovieRun();

	if(runState) {
		vtkMRMLSliceCompositeNode *scn = vtkMRMLSliceCompositeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
		getSliceCompositeNodeRASBounds(scn,rasBoundsRed);
		scn = vtkMRMLSliceCompositeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
		getSliceCompositeNodeRASBounds(scn,rasBoundsYellow);
		scn = vtkMRMLSliceCompositeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
		getSliceCompositeNodeRASBounds(scn,rasBoundsGreen);

		double redMin = rasBoundsRed[4];
		double redMax = rasBoundsRed[5];
		double redStep = 3;
		double redPos = redMin;
		double yellowMin = rasBoundsYellow[0];
		double yellowMax = rasBoundsYellow[1];
		double yellowStep = 3;
		double yellowPos = yellowMin;
		double greenMin = rasBoundsGreen[2];
		double greenMax = rasBoundsGreen[3];
		double greenStep = 3;
		double greenPos = greenMin;

		cout << "movie:\n"
			<< " red:    " << redMin << " .. " << redMax << "\n"
			<< " yellow: " << yellowMin << " .. " << yellowMax << "\n"
			<< " green:  " << greenMin << " .. " << greenMax << "\n"
			<< endl;

		while(runState) {
			//cout << " runRed=" << (runState&1?"true":"false")
			//		<< " runYellow=" << (runState&2?"true":"false")
			//		<< " runGreen=" << (runState&4?"true":"false")
			//		<< " MovieRun=" << runState << endl;

			if(runState&1) {
				sliceNodeRed->JumpSliceByCentering((yellowMin+yellowMax)/2,(greenMin+greenMax)/2,redPos);
				redPos += redStep;
				if(redPos>redMax)  {
					redPos -= redMax - redMin;
					cout << "red Overflow" << endl;
				}
			}
			if(runState&2) {
				sliceNodeYellow->JumpSliceByCentering(yellowPos,(greenMin+greenMax)/2,(redMin+redMax)/2);
				yellowPos += yellowStep;
				if(yellowPos>yellowMax)  {
					yellowPos -= yellowMax - yellowMin;
					cout << "yellow Overflow" << endl;
				}
			}
			if(runState&4) {
				sliceNodeGreen->JumpSliceByCentering((yellowMin+yellowMax)/2,greenPos,(redMin+redMax)/2);
				greenPos += greenStep;
				if(greenPos>greenMax)  {
					greenPos -= greenMax - greenMin;
					cout << "green Overflow" << endl;
				}
			}
			qSlicerApplication::application()->processEvents();

			runState = RegistrationQualityNode->GetMovieRun();
		}
	}
}

//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::Checkerboard(int state) {
	//   Calling checkerboardfilter cli. Logic has been copied and modified from CropVolumeLogic onApply.
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Checkerboard: Invalid scene or parameter set node!");
		return;
	}

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));
	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
// 	vtkMRMLVolumeNode *outputVolume = vtkMRMLVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetCheckerboardVolumeNodeID()));


	if (!state) {
	      	this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}

	if (!this->RegistrationQualityNode->GetCheckerboardVolumeNodeID())
	{
	    if(!this->Internal->VolumesLogic)
	      {
		std::cerr << "CheckerboardPattern: ERROR: failed to get hold of Volumes logic" << std::endl;
		return;
	      }
  //
	    int PatternValue = this->RegistrationQualityNode->GetCheckerboardPattern();
	    std::ostringstream outSS;
	    vtkMRMLScalarVolumeNode *svnode = vtkMRMLScalarVolumeNode::SafeDownCast(referenceVolume);
	    vtkMRMLVolumeNode *outputVolume = NULL;
	    outSS << referenceVolume->GetName() << "_CheckerboardPattern_"<< PatternValue;
	    if(svnode)
	    {
	      outputVolume = this->Internal->VolumesLogic->CloneVolume(this->GetMRMLScene(), referenceVolume, outSS.str().c_str());
	    }
	    else
	    {
	      std::cerr << "Reference volume not scalar volume!" << std::endl;
	      return;
	    }

	  //   outputVolume->SetName(outSS.str().c_str());
	  if (!referenceVolume || !warpedVolume || !outputVolume) {
		  std::cerr << "Volumes not set!" << std::endl;
		  return;
	  }
	  qSlicerCLIModule* checkerboardfilterCLI = dynamic_cast<qSlicerCLIModule*>(
			  qSlicerCoreApplication::application()->moduleManager()->module("CheckerBoardFilter"));
	  QString cliModuleName("CheckerBoardFilter");

	  vtkSmartPointer<vtkMRMLCommandLineModuleNode> cmdNode =
			  checkerboardfilterCLI->cliModuleLogic()->CreateNodeInScene();

	  //Convert PatternValue to string
	  std::ostringstream outPattern;
	  outPattern << PatternValue << "," << PatternValue << "," << PatternValue;
	  // Set node parameters
	  cmdNode->SetParameterAsString("checkerPattern",outPattern.str().c_str());
	  cmdNode->SetParameterAsString("inputVolume1", referenceVolume->GetID());
	  cmdNode->SetParameterAsString("inputVolume2", warpedVolume->GetID());
	  cmdNode->SetParameterAsString("outputVolume", outputVolume->GetID());

	  // Execute synchronously so that we can check the content of the file after the module execution
	  checkerboardfilterCLI->cliModuleLogic()->ApplyAndWait(cmdNode);

	  this->GetMRMLScene()->RemoveNode(cmdNode);

	  outputVolume->SetAndObserveTransformNodeID(NULL);
	  this->RegistrationQualityNode->SetCheckerboardVolumeNodeID(outputVolume->GetID());
	  std::cerr << "Setting checkerboard pattern." << std::endl;
	  return;
	}

	vtkMRMLScalarVolumeNode *checkerboardVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetCheckerboardVolumeNodeID()));
	this->SetForegroundImage(checkerboardVolume,referenceVolume,0);


}
//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::Jacobian(int state) {

	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Jacobian: Invalid scene or parameter set node!");
		return;
	}

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));
	vtkMRMLVectorVolumeNode *vectorVolume = vtkMRMLVectorVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetVectorVolumeNodeID()));

// 	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
// 	vtkMRMLVolumeNode *outputVolume = vtkMRMLVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetCheckerboardVolumeNodeID()));
//


	if (!vectorVolume || !referenceVolume ) {
	    std::cerr << "Volumes not set!" << std::endl;
	    return;
	}

	if (!state) {

	  vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
	      	this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}

	if (!this->RegistrationQualityNode->GetJacobianVolumeNodeID()){

	  if(!this->Internal->VolumesLogic)
	    {
	      std::cerr << "Jacobian: ERROR: failed to get hold of Volumes logic" << std::endl;
	      return;
	    }


	  vtkMRMLScalarVolumeNode *outputVolume = NULL;
	  vtkMRMLScalarVolumeNode *svnode = vtkMRMLScalarVolumeNode::SafeDownCast(referenceVolume);
	  std::ostringstream outSS;

	  outSS << referenceVolume->GetName() << "-jacobian";
	  if(svnode)
	  {
	    outputVolume = this->Internal->VolumesLogic->CloneVolume(this->GetMRMLScene(), referenceVolume, outSS.str().c_str());
	  }
	  else
	  {
	    std::cerr << "Reference volume not scalar volume!" << std::endl;
	    return;
	  }


	qSlicerCLIModule* checkerboardfilterCLI = dynamic_cast<qSlicerCLIModule*>(
			qSlicerCoreApplication::application()->moduleManager()->module("JacobianFilter"));
	QString cliModuleName("JacobianFilter");

	vtkSmartPointer<vtkMRMLCommandLineModuleNode> cmdNode =
			checkerboardfilterCLI->cliModuleLogic()->CreateNodeInScene();

	// Set node parameters
	cmdNode->SetParameterAsString("inputVolume", vectorVolume->GetID());
	cmdNode->SetParameterAsString("outputVolume", outputVolume->GetID());

	// Execute synchronously so that we can check the content of the file after the module execution
	checkerboardfilterCLI->cliModuleLogic()->ApplyAndWait(cmdNode);

	this->GetMRMLScene()->RemoveNode(cmdNode);

	outputVolume->SetAndObserveTransformNodeID(NULL);
	this->RegistrationQualityNode->SetJacobianVolumeNodeID(outputVolume->GetID());

	}

	vtkMRMLScalarVolumeNode *jacobianVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetJacobianVolumeNodeID()));

	jacobianVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeRainbow");
	jacobianVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
	double window=0.8;
	int level=1;

	jacobianVolume->GetScalarVolumeDisplayNode()->SetThreshold(0,3);
	jacobianVolume->GetScalarVolumeDisplayNode()->SetLevel(level);
	jacobianVolume->GetScalarVolumeDisplayNode()->SetWindow(window);

	this->SetForegroundImage(referenceVolume,jacobianVolume,0.5);

	double statisticValues[4]; // 1. Mean 2. STD 3. Max 4. Min
	this->CalculateStatistics(jacobianVolume,statisticValues);

	this->RegistrationQualityNode->DisableModifiedEventOn();
	this->RegistrationQualityNode->SetJacobianStatistics( statisticValues );
// 	this->RegistrationQualityNode->SetJacobianSTD( statisticValues[1] );
	this->RegistrationQualityNode->DisableModifiedEventOff();

	return;
}
//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::InverseConsist(int state) {

	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Inverse Consistency: Invalid scene or parameter set node!");
		return;
	}

	vtkMRMLVectorVolumeNode *vectorVolume1 = vtkMRMLVectorVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetVectorVolumeNodeID()));
	vtkMRMLVectorVolumeNode *vectorVolume2 = vtkMRMLVectorVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetInvVectorVolumeNodeID()));
	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));

// 	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
// 	vtkMRMLVolumeNode *outputVolume = vtkMRMLVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetCheckerboardVolumeNodeID()));
//
	if (!vectorVolume1 || !vectorVolume2 || !referenceVolume ) {
	    std::cerr << "Volumes not set!" << std::endl;
	    return;
	}

	if (!state) {

	  vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
	      	this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}

	if (!this->RegistrationQualityNode->GetInverseConsistVolumeNodeID()){
	  if(!this->Internal->VolumesLogic)
	    {
	      std::cerr << "Inverse Consistency: ERROR: failed to get hold of Volumes logic" << std::endl;
	      return;
	    }


	  vtkMRMLScalarVolumeNode *outputVolume = NULL;
	  vtkMRMLScalarVolumeNode *svnode = vtkMRMLScalarVolumeNode::SafeDownCast(referenceVolume);
	  std::ostringstream outSS;

	  outSS << referenceVolume->GetName() << "-inverseConsist";
	  if(svnode)
	  {
	    outputVolume = this->Internal->VolumesLogic->CloneVolume(this->GetMRMLScene(), referenceVolume, outSS.str().c_str());
	  }
	  else
	  {
	    std::cerr << "Reference volume not scalar volume!" << std::endl;
	    return;
	  }


	qSlicerCLIModule* checkerboardfilterCLI = dynamic_cast<qSlicerCLIModule*>(
			qSlicerCoreApplication::application()->moduleManager()->module("InverseConsistency"));
	QString cliModuleName("InverseConsistency");

	vtkSmartPointer<vtkMRMLCommandLineModuleNode> cmdNode =
			checkerboardfilterCLI->cliModuleLogic()->CreateNodeInScene();

	// Set node parameters
	cmdNode->SetParameterAsString("inputVolume1", vectorVolume1->GetID());
	cmdNode->SetParameterAsString("inputVolume2", vectorVolume2->GetID());
	cmdNode->SetParameterAsString("outputVolume", outputVolume->GetID());

	// Execute synchronously so that we can check the content of the file after the module execution
	checkerboardfilterCLI->cliModuleLogic()->ApplyAndWait(cmdNode);

	this->GetMRMLScene()->RemoveNode(cmdNode);

	outputVolume->SetAndObserveTransformNodeID(NULL);
	this->RegistrationQualityNode->SetInverseConsistVolumeNodeID(outputVolume->GetID());
	}

	vtkMRMLScalarVolumeNode *inverseConsistVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetInverseConsistVolumeNodeID()));


	inverseConsistVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeGreen");
	inverseConsistVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
	double window=10;
	int level=5;

	inverseConsistVolume->GetScalarVolumeDisplayNode()->SetThreshold(0,10);
	inverseConsistVolume->GetScalarVolumeDisplayNode()->SetLevel(level);
	inverseConsistVolume->GetScalarVolumeDisplayNode()->SetWindow(window);

	this->SetForegroundImage(referenceVolume,inverseConsistVolume,0.5);

	double statisticValues[4]; // 1. Mean 2. STD 3. Max 4. Min
	this->CalculateStatistics(inverseConsistVolume,statisticValues);

	this->RegistrationQualityNode->DisableModifiedEventOn();
	this->RegistrationQualityNode->SetInverseConsistStatistics( statisticValues );
	this->RegistrationQualityNode->DisableModifiedEventOff();

	return;
}
//--- Default mode when checkbox is unchecked -----------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::SetDefaultDisplay(vtkMRMLScalarVolumeNode *backgroundVolume, vtkMRMLScalarVolumeNode *foregroundVolume) {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("SetDefaultDisplay: Invalid scene or parameter set node!");
		return;
	}


	if (!backgroundVolume || !foregroundVolume) {
		vtkErrorMacro("SetDefaultDisplay: Invalid volumes!");
		return;
	}
	//TODO: Volumes go back to gray value - perhaps we should rembemer previous color settings?
	 backgroundVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeGrey");
	 foregroundVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeGrey");

	// Set window and level the same for warped and reference volume.
	foregroundVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
// 	double window, level;
// 	window = backgroundVolume->GetScalarVolumeDisplayNode()->GetWindow();
// 	level = backgroundVolume->GetScalarVolumeDisplayNode()->GetLevel();
	foregroundVolume->GetScalarVolumeDisplayNode()->SetWindow(backgroundVolume->GetScalarVolumeDisplayNode()->GetWindow());
	foregroundVolume->GetScalarVolumeDisplayNode()->SetLevel(backgroundVolume->GetScalarVolumeDisplayNode()->GetLevel());
	this->SetForegroundImage(backgroundVolume,foregroundVolume,0.5);

	return;
}
//---Calculate Statistic of image-----------------------------------
void vtkSlicerRegistrationQualityLogic::CalculateStatistics(vtkMRMLScalarVolumeNode *inputVolume, double statisticValues[4] )
{
  vtkNew<vtkImageAccumulate> StatImage;
  StatImage->SetInput(inputVolume->GetImageData());
  StatImage->Update();
  statisticValues[0]= StatImage->GetMean()[0];
  statisticValues[1]= StatImage->GetStandardDeviation()[0];
  statisticValues[2]= StatImage->GetMax()[0];
  statisticValues[3]= StatImage->GetMin()[0];
}


//---Change backroung image and set opacity-----------------------------------
void vtkSlicerRegistrationQualityLogic
::SetForegroundImage(vtkMRMLScalarVolumeNode *backgroundVolume, vtkMRMLScalarVolumeNode *foregroundVolume,double opacity) {
		// Set warped image in background

	if (!backgroundVolume || !foregroundVolume) {
		vtkErrorMacro("SetForegroundImage: Invalid reference or warped volume!");
		return;
	}
	qSlicerApplication * app = qSlicerApplication::application();
	qSlicerLayoutManager * layoutManager = app->layoutManager();

	if (!layoutManager) {
		return;
	}

	vtkMRMLSliceCompositeNode *rcn = vtkMRMLSliceCompositeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
	vtkMRMLSliceCompositeNode *ycn = vtkMRMLSliceCompositeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
	vtkMRMLSliceCompositeNode *gcn = vtkMRMLSliceCompositeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));

	vtkMRMLSliceLogic* redSliceLogic = rcn != NULL ?
			GetMRMLApplicationLogic()->GetSliceLogicByLayoutName(rcn->GetLayoutName()) : NULL;
	if (redSliceLogic == NULL) {
		vtkErrorMacro("SetForegroundImage: Invalid SliceLogic!");
		return;
	}
	// Link Slice Controls
	rcn->SetLinkedControl(1);
	ycn->SetLinkedControl(1);
	gcn->SetLinkedControl(1);
	// Set volumes and opacity for all three layers.

// 	redSliceLogic->StartSliceCompositeNodeInteraction();
	rcn->SetBackgroundVolumeID(backgroundVolume->GetID());
	rcn->SetForegroundVolumeID(foregroundVolume->GetID());
	rcn->SetForegroundOpacity(opacity);
	redSliceLogic->StartSliceCompositeNodeInteraction(
		vtkMRMLSliceCompositeNode::ForegroundVolumeFlag
		| vtkMRMLSliceCompositeNode::BackgroundVolumeFlag);
	redSliceLogic->EndSliceCompositeNodeInteraction();
	return;
}
//----------------------------------------------------------------------------
