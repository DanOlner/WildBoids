/*
 * AngleFromTwoPoints.java
 *
 * Created on 29 May 2007, 19:33
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package boids;

import java.awt.geom.Point2D;

/**
 *
 * @author Olner Dan
 */
public class AngleFromTwoPoints {
    
    /** Creates a new instance of AngleFromTwoPoints *
     * Takes in two points and gives back the atan angle,
     * adjusted for the correct direction
     */
    public double getAngleFromPoints (Point2D.Double p1, Point2D.Double p2) {
        
        double angle;
        int rule=0;
        
        //first calculate the x and ys
        
        //double x = p1.x - p2.x;
        //double y = p1.y  - p2.y;
        
        double x = p2.x - p1.x;
        double y = p2.y - p1.y;
        
        angle = (Math.atan(y/x));
        
        //Now we need to adjust based on which way the points are -
        //presuming point one is 'me'...
        
        //if ((x>0 && y<0) || (x>0 && y>0)) {angle+=(Math.PI);} else if (x<0 && y>0) {angle+=(Math.PI*2);}
        
        if ((x>0 && y<0) || (x>0 && y>0)) {angle+=(Math.PI*2);} else if (x<0 && y>0) {angle+=(Math.PI);}
        
        /*
        switch (rule) {
            
                //e.g. 45 degree angle pointing SE - atan returns the correct answer
            case 0: break;
                //e.g. 45 degree angle pointing SW - atan returns the correct answer
            case 1: ; break;
                //e.g. 45 degree angle pointing NW - atan returns the correct answer
            case 2: angle+=(Math.PI); break;
                //e.g. 45 degree angle pointing NE - atan returns the correct answer
            case 3: ; break;
                
        }
        */
        return angle;
        
    }
    
    
    
    public double getAngleFromXY (double x, double y) {
        
        double angle;
        int rule=0;
        
        x=-x;
        y=-y;
        
        angle = (Math.atan(y/x));
        
        //Now we need to adjust based on which way the points are -
        //presuming point one is 'me'...
        
        if ((x>0 && y<0) || (x>0 && y>0)) {angle+=(Math.PI);} else if (x<0 && y>0) {angle+=(Math.PI*2);}
        
        //angle+=(Math.PI);
        
        //angle%=(Math.PI*2);
        
        /*
        switch (rule) {
            
                //e.g. 45 degree angle pointing SE - atan returns the correct answer
            case 0: break;
                //e.g. 45 degree angle pointing SW - atan returns the correct answer
            case 1: ; break;
                //e.g. 45 degree angle pointing NW - atan returns the correct answer
            case 2: angle+=(Math.PI); break;
                //e.g. 45 degree angle pointing NE - atan returns the correct answer
            case 3: ; break;
                
        }
        */
        return angle;
        
    }
    
    
    
}
