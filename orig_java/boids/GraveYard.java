/*
 * GraveYard.java
 *
 * Created on 27 May 2007, 14:50
 *
 * GraveYard stores recent boid deaths, allowing 
 * DrawPanel to know so's it can plot the position of their
 * deaths for a few turns (rather than them just disappearing
 * from the screen un-mourned.)
 */

package boids;

import java.util.ArrayList;

/**
 *
 * @author Olner Dan
 */
public class GraveYard {
    
    //A dead boid's final resting place... 
    ArrayList<Boid> deadBoids;
    
    /** Creates a new instance of DeathMarker */
    public GraveYard() {
    }
    
    public void addDeadBoid(Boid db) {
        
        deadBoids.add(db);
        
    }
    
    public ArrayList getDeadBoids () {
        
        return deadBoids;

    }
    
    
}
