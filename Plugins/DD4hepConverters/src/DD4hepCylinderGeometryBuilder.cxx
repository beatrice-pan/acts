#include "ACTS/Plugins/DD4hepConverters/DD4hepCylinderGeometryBuilder.h"
// Geometry module
#include "ACTS/Detector/TrackingGeometry.h"
#include "ACTS/Detector/TrackingVolume.h"
// DD4hepPlugin
#include "ACTS/Plugins/DD4hepConverters/DD4hepGeometryHelper.h"
#include "ACTS/Plugins/DD4hepConverters/DD4hepLayerHelper.h"

Acts::DD4hepCylinderGeometryBuilder::DD4hepCylinderGeometryBuilder() :
m_DD4hepGeometrySvc(nullptr),
m_volumeBuilder(nullptr),
m_volumeHelper(nullptr),
m_layerHelper(new Acts::DD4hepLayerHelper())
{}

Acts::DD4hepCylinderGeometryBuilder::~DD4hepCylinderGeometryBuilder()
{
    delete m_layerHelper;
}

std::unique_ptr<Acts::TrackingGeometry> Acts::DD4hepCylinderGeometryBuilder::trackingGeometry() const
{
    // the return geometry -- and the highest volume
    std::unique_ptr<Acts::TrackingGeometry> trackingGeometry = nullptr;
    Acts::TrackingVolumePtr    highestVolume = nullptr;
    Acts::TrackingVolumePtr beamPipeVolume   = nullptr;
    
    // get the DD4hep world detector element
    DD4hep::Geometry::DetElement detWorld = m_DD4hepGeometrySvc->worldDetElement();
    // get the sub detectors
    std::vector<DD4hep::Geometry::DetElement> detElements;
    const DD4hep::Geometry::DetElement::Children& children = detWorld.children();
    for (auto& detElement : children) detElements.push_back(detElement.second);
    //sort by id to build detector from bottom to top
    sort(detElements.begin(),detElements.end(),
         [](const DD4hep::Geometry::DetElement& a,
            const DD4hep::Geometry::DetElement& b) {
             return (a.id()<b.id());}
         );
    // loop over the volumes
    for (auto& detElement : detElements) {
        if (detElement.type()=="beamtube") {
            //MSG_DEBUG("BeamPipe is being built");
            //extract material
            DD4hep::Geometry::Material mat = detElement.volume().material();
            //create the tracking volume
            beamPipeVolume = Acts::TrackingVolume::create(Acts::DD4hepGeometryHelper::extractTransform(detElement),Acts::DD4hepGeometryHelper::extractVolumeBounds(detElement),Acts::Material(mat.radLength(),mat.intLength(),mat.A(),mat.Z(),mat.density()),nullptr,nullptr,nullptr,nullptr,"BeamTube");
        }
        else {
        // assign a new highest volume (and potentially wrap around the given highest volume so far)
            const LayerTriple* layerTriple = m_layerHelper->createLayerTriple(detElement);
            highestVolume = m_volumeBuilder->trackingVolume(highestVolume,Acts::DD4hepGeometryHelper::extractVolumeBounds(detElement),layerTriple,m_layerHelper->volumeTriple());
        }
    }
    // if you have a highest volume, stuff it into a TrackingGeometry
    if (highestVolume) {
        // see if the beampipe needs to be wrapped
        if (beamPipeVolume) highestVolume = m_volumeHelper->createContainerTrackingVolume({beamPipeVolume,highestVolume});
        
        // create the TrackingGeometry
        trackingGeometry = std::make_unique<Acts::TrackingGeometry>(highestVolume);
    }
    // return the geometry to the service
    return trackingGeometry;
}
