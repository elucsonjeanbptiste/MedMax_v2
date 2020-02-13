#include "viewerfibula.h"

ViewerFibula::ViewerFibula(QWidget *parent, StandardCamera *camera, int sliderMax, int fibulaOffset) : Viewer (parent, camera, sliderMax)
{
    indexOffset = 0;
    maxOffset = fibulaOffset;
    isPlanesRecieved = false;
    isCutSignal = false;
}

void ViewerFibula::initSignals(){
    connect(&mesh, &Mesh::sendInfoToManible, this, &ViewerFibula::recieveFromFibulaMesh);
}

void ViewerFibula::recieveFromFibulaMesh(std::vector<int> planes, std::vector<Vec> verticies, std::vector<std::vector<int>> triangles, std::vector<int> colours, std::vector<Vec> normals, int nbColours){
    std::vector<Vec> polylineInPlanes = getPolyline();

    Q_EMIT sendToManible(planes, verticies, triangles, polylineInPlanes, colours, normals, nbColours);

    //polyline.clear();       // to stop it from being drawn
}

std::vector<Vec> ViewerFibula::getPolyline(){
    std::vector<Vec> polylineInPlanes;
    Vec v;

    createPolyline();

    // Get the polyline vector in relation to the planes (in order of the planes)
    v = leftPlane->getPolylineVector(polyline[1]);
    polylineInPlanes.push_back(v);

    for(unsigned int i=0; i<ghostPlanes.size(); i++){       // +1 offset for the left plane
        if(i%2==0) v = ghostPlanes[i]->getPolylineVector(polyline[i]);     // even: look behind
        else v = ghostPlanes[i]->getPolylineVector(polyline[i+2]);       // odd : look forward
        polylineInPlanes.push_back(v);
    }

    v = rightPlane->getPolylineVector(polyline[polyline.size()-2]);
    polylineInPlanes.push_back(v);

    return polylineInPlanes;
}

void ViewerFibula::createPolyline(){
    polyline.clear();

    polyline.push_back(leftPlane->getPosition());
    for(unsigned int i=0; i<ghostPlanes.size(); i++){
        polyline.push_back(ghostPlanes[i]->getPosition());
    }

    polyline.push_back(rightPlane->getPosition());
}

void ViewerFibula::repositionPlanes(std::vector<Vec> polyline, std::vector<Vec> axes){
    resetMandibleInfo(polyline, axes);
    setPlanePositions();
    setPlaneOrientations();
    update();
}

void ViewerFibula::resetMandibleInfo(std::vector<Vec> polyline, std::vector<Vec> axes){
    mandiblePolyline.clear();
    mandiblePolyline = polyline;
    mandibleAxes.clear();
    mandibleAxes = axes;
}

void ViewerFibula::setPlanePositions(){
    leftPlane->setPosition(curve->getPoint( static_cast<unsigned int>(static_cast<int>(curveIndexL) + indexOffset)));
    rightPlane->setPosition(curve->getPoint(static_cast<unsigned int>(static_cast<int>(curveIndexR) + indexOffset)));
    for(unsigned int i=0; i<ghostPlanes.size(); i++){
        ghostPlanes[i]->setPosition(curve->getPoint(static_cast<unsigned int>(static_cast<int>(ghostLocation[i]) + indexOffset)));
    }
}

void ViewerFibula::setPlaneOrientations(){
    if(mandiblePolyline.size()==0) return;

    // This step has to be done for at least the director in order to give it a base from which to rotate

    // Orientate the left plane
    Vec normal = leftPlane->getNormal();        // The normal defines the polyline, so move our polyline to the mandible polyline
    Quaternion s = Quaternion(-normal, mandiblePolyline[0]);  // -normal so it doesnt do a 180 flip (a rotation of the normal to the polyline)
    leftPlane->setOrientation(s.normalized());

    // Orientate the ghost planes
    for(unsigned int i=0; i<ghostPlanes.size(); i++){
        normal = ghostPlanes[i]->getNormal();
        s = Quaternion(normal, mandiblePolyline[i]);
        ghostPlanes[i]->setOrientation(s.normalized());
    }

    // Orientate the right plane
    normal = rightPlane->getNormal();
    s = Quaternion(normal, mandiblePolyline[mandiblePolyline.size()-1]);
    rightPlane->setOrientation(s.normalized());

    if(ghostPlanes.size()==0){
        // Project the mand and the fib on the left plane
        std::vector<Vec> fibulaPolyline = getPolyline();
        fibulaPolyline = getPolyline();
        Vec mandPoint = rightPlane->getLocalProjection(mandiblePolyline[1]);
        Vec fibPoint = rightPlane->getLocalProjection(fibulaPolyline[1]);
        mandPoint.normalize();  // normalise them so they have the same length
        fibPoint.normalize();

        double alpha = angle(mandPoint, fibPoint)+ M_PI;    // Get the angle between the two
        Vec axis = Vec(0,0,1);
        //leftPlane->rotatePlane(axis, alpha);
        rightPlane->rotatePlane(axis, alpha);


    }

    else{
        Vec axis = Vec(0,0,1);
        std::vector<Vec> fibulaPolyline = getPolyline();

        // Left
        Vec mandPoint = leftPlane->getLocalProjection(mandiblePolyline[0]);
        Vec fibPoint = leftPlane->getLocalProjection(fibulaPolyline[0]);
        mandPoint.normalize();
        fibPoint.normalize();
        double alpha = angle(mandPoint, fibPoint) + M_PI;

        leftPlane->rotatePlane(axis, alpha);
        ghostPlanes[0]->rotatePlane(axis, alpha);

        // Ghost
        for(unsigned int i=1; i<ghostPlanes.size()-2; i+=2){
            mandPoint = ghostPlanes[i]->getLocalProjection(mandiblePolyline[i+1]);
            fibPoint = ghostPlanes[i]->getLocalProjection(fibulaPolyline[i+1]);
            mandPoint.normalize();
            fibPoint.normalize();
            alpha = angle(mandPoint, fibPoint) + M_PI;

            ghostPlanes[i]->rotatePlane(axis, alpha);
            ghostPlanes[i+1]->rotatePlane(axis, alpha);
        }

        // Right
        unsigned int lastIndex = mandiblePolyline.size()-1;
        mandPoint = rightPlane->getLocalProjection(mandiblePolyline[lastIndex]);
        fibPoint = rightPlane->getLocalProjection(fibulaPolyline[lastIndex]);
        mandPoint.normalize();
        fibPoint.normalize();
        alpha = angle(mandPoint, fibPoint) + M_PI;

        rightPlane->rotatePlane(axis, alpha);
        ghostPlanes[lastIndex-2]->rotatePlane(axis, alpha); // the last ghost plane
    }

    Q_EMIT requestAxes();
}

// Rotate the end plane to match the mandibule
void ViewerFibula::recieveTest(std::vector<Vec> axes, std::vector<double> angles){
    if(ghostPlanes.size()==0){
        Vec x = rightPlane->getMeshVectorFromLocal(axes[0]);        // axes must stay in mesh coordinates
        x.normalize();
        Vec y = rightPlane->getMeshVectorFromLocal(axes[1]);
        y.normalize();
        Vec z = rightPlane->getMeshVectorFromLocal(axes[2]);
        z.normalize();
        leftPlane->setFrameFromBasis(x,y,z);
    }
    else{
        Vec x = leftPlane->getMeshVectorFromLocal(axes[0]);        // axes must stay in mesh coordinates
        x.normalize();
        Vec y = leftPlane->getMeshVectorFromLocal(axes[1]);
        y.normalize();
        Vec z = leftPlane->getMeshVectorFromLocal(axes[2]);
        z.normalize();
        ghostPlanes[0]->setFrameFromBasis(x,y,z);

        Vec x2 = leftPlane->getMeshVectorFromLocal(axes[3]);        // axes must stay in mesh coordinates
        x2.normalize();
        Vec y2 = leftPlane->getMeshVectorFromLocal(axes[4]);
        y2.normalize();
        Vec z2 = leftPlane->getMeshVectorFromLocal(axes[5]);
        z2.normalize();
        ghostPlanes[1]->setFrameFromBasis(x2,y2,z2);

        std::vector<Vec> poly = getPolyline();
        Vec axisX = Vec(1,0,0);
        Vec axisZ = Vec(0,0,1);
        Vec rotationAxis = cross(axisX, poly[2]);

        Vec a = getCustomProjection(poly[2], axisX);
        a.normalize();
        Vec b = getCustomProjection(axisZ, axisX);
        double alpha = angle(a,b);

        double theta = - alpha + angles[0] + M_PI;
        ghostPlanes[1]->rotatePlane(rotationAxis, theta);

        for(unsigned int i=2; i<ghostPlanes.size()-1; i+=2){
            x = ghostPlanes[i-1]->getMeshVectorFromLocal(axes[3*i]);        // axes must stay in mesh coordinates
            x.normalize();
            y = ghostPlanes[i-1]->getMeshVectorFromLocal(axes[3*i+1]);
            y.normalize();
            z = ghostPlanes[i-1]->getMeshVectorFromLocal(axes[3*i+2]);
            z.normalize();
            ghostPlanes[i]->setFrameFromBasis(x,y,z);

            x2 = ghostPlanes[i-1]->getMeshVectorFromLocal(axes[3*i+3]);        // axes must stay in mesh coordinates
            x2.normalize();
            y2 = ghostPlanes[i-1]->getMeshVectorFromLocal(axes[3*i+4]);
            y2.normalize();
            z2 = ghostPlanes[i-1]->getMeshVectorFromLocal(axes[3*i+5]);
            z2.normalize();
            ghostPlanes[i+1]->setFrameFromBasis(x2,y2,z2);
        }

        unsigned int lastIndex = ghostPlanes.size()-1;
        unsigned int lastAxe = axes.size()-3;

        x = ghostPlanes[lastIndex]->getMeshVectorFromLocal(axes[lastAxe]);        // axes must stay in mesh coordinates
        x.normalize();
        y = ghostPlanes[lastIndex]->getMeshVectorFromLocal(axes[lastAxe+1]);
        y.normalize();
        z = ghostPlanes[lastIndex]->getMeshVectorFromLocal(axes[lastAxe+2]);
        z.normalize();
        rightPlane->setFrameFromBasis(x,y,z);
    }
}

// Move all planes by the same offset (right plane INCLUDED) - when the slider is dragged
void ViewerFibula::movePlanes(int position){
    int offset = static_cast<int>(static_cast<double>(position)/ static_cast<double>(maxOffset) * static_cast<double>(nbU));

    // Check that it this offset doesn't exceed the size of the fibula
    if(static_cast<int>(curveIndexL) + offset < static_cast<int>(nbU) && static_cast<int>(curveIndexL) + offset > 0 && static_cast<int>(curveIndexR) + offset < static_cast<int>(nbU) && static_cast<int>(curveIndexR) + offset > 0){
        indexOffset = offset;
        repositionPlanes(mandiblePolyline, mandibleAxes);
    }

    mesh.setTransfer(false);
    mesh.updatePlaneIntersections();
}

void ViewerFibula::planesMoved(){
    mesh.setTransfer(true);
    mesh.sendToManible();
}

// Add the ghost planes (this should only be called once)
void ViewerFibula::addGhostPlanes(int nb){
    for(unsigned int i=0; i<ghostPlanes.size(); i++) delete ghostPlanes[i];     // remove any ghost planes
    ghostPlanes.clear();

    for(unsigned int i=0; i<static_cast<unsigned int>(nb); i++){
        ghostPlanes.push_back(new Plane(25.0, Movable::STATIC));

        // If we're too far along the fibula, take it all back
        int overload = static_cast<int>(ghostLocation[i]) + indexOffset - static_cast<int>(curve->getNbU()) + 1;   // The amount by which the actual index passes the end of the curve
        if(overload > 0) indexOffset -= overload;
    }
}

// Find the locations of the ghost planes from the distances from the planes in the mandible
void ViewerFibula::findGhostLocations(unsigned int nb, double distance[]){
    ghostLocation.clear();

    unsigned int index = curve->indexForLength(curveIndexL, distance[0]);
    ghostLocation.push_back(index);
    unsigned int nextIndex = curve->indexForLength(index, 40.0);        // 30 is a tempory security margin
    ghostLocation.push_back(nextIndex);     // the mirror plane

    for(unsigned int i=1; i<static_cast<unsigned int>(nb); i++){
        index = curve->indexForLength(ghostLocation[2*i-1], distance[i]);
        ghostLocation.push_back(index);
        unsigned int nbU = curve->getNbU();
        nextIndex = curve->indexForLength(index, 40.0);
        if((nextIndex)<nbU) ghostLocation.push_back(nextIndex);
        else ghostLocation.push_back(nbU-1);

    }
    curveIndexR = curve->indexForLength(ghostLocation[2*static_cast<unsigned int>(nb)-1], distance[nb]);        // place the right plane after the last ghost plane (left plane doesn't move)
}

// NOT USED
void ViewerFibula::matchToMandibleFrame(Plane* p1, Plane* p2, Vec a, Vec b, Vec c, Vec x, Vec y, Vec z){
    p1->setFrameFromBasis(a,b,c);
    p2->setFrameFromBasis(x,y,z);
}

// Don't wait for ghost planes, go ahead and cut
void ViewerFibula::noGhostPlanesToRecieve(){
    isPlanesRecieved = true;
    handleCut();
}

// Add ghost planes that correspond to the ghost planes in the jaw
void ViewerFibula::ghostPlanesRecieved(unsigned int nb, double distance[], std::vector<Vec> mandPolyline, std::vector<Vec> axes){  
    if(nb==0){      // if no ghost planes were actually recieved
        for(unsigned int i=0; i<ghostPlanes.size(); i++) delete ghostPlanes[i];
        ghostPlanes.clear();        // TODO look at this (call noGhostPlanesToRecieve?)
        mesh.deleteGhostPlanes();
        return;
    }

    unsigned int oldNb = static_cast<unsigned int>(ghostPlanes.size() / 2);

    findGhostLocations(nb, distance);

    addGhostPlanes(2* static_cast<int>(nb));    // 2*nb ghost planes : there are 2 angles for each plane in the manible, so twice the number of ghost planes

    repositionPlanes(mandPolyline, axes);

    // If its cut and the number of planes has changed
    if(mesh.getIsCut() && nb!=oldNb){
        mesh.deleteGhostPlanes();       // should update the number of planes here?
        //isPlanesRecieved = true;
        cutMesh();
        //return;
    }

    isPlanesRecieved = true;
    handleCut();
}

// When we want to move the right plane (the right plane is moved in the jaw)
void ViewerFibula::movePlaneDistance(double distance, std::vector<Vec> mandPolyline, std::vector<Vec> axes){
    unsigned int newIndex;

    if(ghostPlanes.size()==0) newIndex = curve->indexForLength(curveIndexL, distance);
    else newIndex = curve->indexForLength(ghostLocation[ghostPlanes.size()-1], distance);

    if(static_cast<unsigned int>(static_cast<int>(newIndex) + indexOffset) >= nbU) return;      // This should never happen
    else curveIndexR = newIndex;

    repositionPlanes(mandPolyline, axes);

    mesh.updatePlaneIntersections(rightPlane);
}

// One of the ghost planes is moved in the jaw
// NOT USED FOR NOW
void ViewerFibula::middlePlaneMoved(unsigned int nb, double distances[], std::vector<Vec> mandPolyline, std::vector<Vec> axes){
    if(nb==0) return;

    findGhostLocations(nb, distances);

    // If we're too far along the fibula, take it all back
    int overload = static_cast<int>(curveIndexR) + indexOffset - static_cast<int>(curve->getNbU()) + 1;   // The amount by which the actual index passes the end of the curve
    if(overload > 0){
        indexOffset -= overload;
        Q_EMIT setPlaneSliderValue(static_cast<int>( (static_cast<double>(indexOffset)/static_cast<double>(nbU)) * static_cast<double>(maxOffset) ));
    }

    repositionPlanes(mandPolyline, axes);

    mesh.updatePlaneIntersections(rightPlane);
}

// Initialise the curve that the planes follow (to eventually be changed to automatically calculate the points)
void ViewerFibula::initCurve(){
    const long nbCP = 6;
    std::vector<Vec> control;

    control.push_back(Vec(108.241, 69.6891, -804.132));
    control.push_back(Vec(97.122, 82.1788, -866.868));
    control.push_back(Vec(93.5364, 90.1045, -956.126));
    control.push_back(Vec(83.3966, 92.5807, -1069.7));
    control.push_back(Vec(80.9, 90.1, -1155));
    control.push_back(Vec(86.4811, 90.9929, -1199.7));

    curve = new Curve(nbCP, control);

    nbU = 1500;

    int nbSeg = nbCP-3;
    nbU -= static_cast<unsigned int>(static_cast<int>(nbU)%nbSeg);

    curve->generateCatmull(nbU);
    connect(curve, &Curve::curveReinitialised, this, &Viewer::updatePlanes);

    initPlanes(Movable::STATIC);
}

void ViewerFibula::cutMesh(){
    isCutSignal = true;
    handleCut();
}

void ViewerFibula::handleCut(){
    if(isCutSignal && isPlanesRecieved){
        // delete ghost planes first?
        for(unsigned int i=0; i<ghostPlanes.size(); i++){
            mesh.addPlane(ghostPlanes[i]);
        }

        if(ghostPlanes.size()==0) mesh.setIsCut(Side::EXTERIOR, true, true);    // call the update if an exterior plane isn't going to
        else mesh.setIsCut(Side::EXTERIOR, true, false);

        isGhostPlanes = true;
        isCutSignal = false;
        isPlanesRecieved = false;
    }
}

void ViewerFibula::uncutMesh(){
    isPlanesRecieved = false;
    mesh.setIsCut(Side::EXTERIOR, false, false);
    isGhostPlanes = false;
    for(unsigned int i=0; i<ghostPlanes.size(); i++) delete ghostPlanes[i];
    ghostPlanes.clear();
    update();
}


void ViewerFibula::handleMovementStart(){

}

void ViewerFibula::handleMovementEnd(){

}
