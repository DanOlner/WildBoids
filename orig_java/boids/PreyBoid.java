/*
 * PreyBoid.java
 *
 * Created on 25 May 2007, 17:31
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package boids;

import java.util.ArrayList;

/**
 *
 * @author Olner Dan
 */
public class PreyBoid extends Boid {
    
    
    private ArrayList<Boid> whoWantsToEatMe = new ArrayList<Boid>();
    
    
    /** Creates a new instance of PreyBoid
     * if its the start of evolution, the superclass will fill
     * the boid's genes with random values...
     */
    
    public PreyBoid(int id, int[] cageXY, Evolution evolution, int turningSpeed, int[] fovSize) {
        
        super(id, cageXY, evolution, turningSpeed, fovSize);
        
    }
    
    /*This is the constructor for birthing a new Boid...
     */
    public PreyBoid(boolean newBoid, int id, int[] cageXY, Evolution evolution, int[] fovSize) {
        
        super(newBoid, id, cageXY, evolution, fovSize);
        
    }
    
    //allows others to check whether this is a predator or prey
    public boolean isPredator() {
        
        return false;
        
    }
    
    
    //For Predators to 'register their interest' in eating me!
    public void iAmDinnerForWho(Boid b) {
        
        whoWantsToEatMe.add(b);
        
    }
    
    
    
    
    //Things to do before the next round
    public void nextMoment() {
        
        //First things first, see if I have to choose between preyboids wanting to eat me...
        if (!whoWantsToEatMe.isEmpty() ) {
            
            if (id==0) {System.out.println("I'm dead!");}
            
            //if there's only one, that's easy enough...
            //if (whoWantsToEatMe.size() == 1) {
            
            PreyBoid babyBoid = new PreyBoid(true, id, cageXY, evolution, fovSize);
            
            evolution.EvolveNow(babyBoid, this);
            
            //tell the appropriate PredatorBoid to have a chomp - 
            whoWantsToEatMe.get(0).eat();
            //System.out.println("I've been chomped!");
            
            //otherwise, I have to choose one. Maybe I'll implement something later to work out distance, but
            //in this case I think we'll just do 'first come first serve...'
            //System.out.println("Current direction:" + currentDirection + ", last direction:"+lastDirection);
            
            
        }
        
        
        //Reset all the Field of View arrays for the next go
        //I forgot to do this before, and the poor little buggers have a right fit...
        //empty all the dirRule seens
        for (DirectionRule d : dirRule) {
            
            d.emptyBoidsInView();
            
        }
        
        age++;
        
        whoWantsToEatMe.clear();
        
    }
    
    
    
    //overridden predator methods
    public void distanceToScoff() {}
    
    public void eat() {}
    
    
}
