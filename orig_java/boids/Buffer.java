/*
 * Buffer.java
 *
 * Created on 27 May 2007, 18:19
 *
 * Stores four rectangles that represent a buffer zone in the BoidCage:
 * Used for working out when to check of a boid's field of vision needs
 * to stretch round the torus...
 */

package boids;

import java.awt.geom.Rectangle2D;

/**
 *
 * @author Olner Dan
 */
public class Buffer {
    
    public Rectangle2D.Double RectA;
    public Rectangle2D.Double RectB;
    public Rectangle2D.Double RectC;
    public Rectangle2D.Double RectD;
    
    /** Creates a new instance of Buffer */
    public Buffer(int[] cageXY, int fov) {
        
        //top rectangle
        RectA = new Rectangle2D.Double(0,0,cageXY[0],fov);
        //left rectangle
        RectB = new Rectangle2D.Double(0,0,fov,cageXY[1]);
        //bottom rectangle
        RectC = new Rectangle2D.Double(0,cageXY[1]-fov,cageXY[0],fov);
        //right rectangle
        RectD = new Rectangle2D.Double(cageXY[0]-fov,0,fov,cageXY[1]);
                
    }
    
   
    
}
