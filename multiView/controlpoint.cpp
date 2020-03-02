#include "controlpoint.h"
#include <math.h>

#include <QGLViewer/manipulatedFrame.h>

ControlPoint::ControlPoint(Vec p)
{
    this->p = p;
    initialise();
    isSwitchFrames = true;
}

ControlPoint::ControlPoint(double x, double y, double z)
{
    this->p = Vec(x,y,z);
    initialise();
}

void ControlPoint::initialise(){
    mf = ManipulatedFrame();
    mf.setPosition(this->p.x, this->p.y, this->p.z);
    connect(&mf, &ManipulatedFrame::manipulated, this, &ControlPoint::cntrlMoved);
}

// Call this to move a point without setting off a signal to update
void ControlPoint::moveControlPoint(Vec newPos){
   /* p.x = newPos.x;
    p.y = newPos.y;
    p.z = newPos.z;

    mf.setPosition(this->p.x, this->p.y, this->p.z);*/
}

void ControlPoint::draw(){

    if(isSwitchFrames){
        glPushMatrix();
        glMultMatrixd(mf.matrix());
    }

    if(mf.grabsMouse()) glColor3f(0, 1, 1);
    else glColor3f(0.6f, 0, 0.4f);

    glPointSize(10.0);
    glBegin(GL_POINTS);
        glVertex3d(0, 0, 0);
    glEnd();

    glPointSize(1.0);
    glColor3f(1.0,1.0,1.0);

    if(isSwitchFrames) glPopMatrix();
}

void ControlPoint::cntrlMoved(){
    double x,y,z;
    mf.getPosition(x,y,z);
    p.x = x;
    p.y = y;
    p.z = z;

    Q_EMIT ControlPoint::cntrlPointTranslated();
}
