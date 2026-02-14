/*
 * PreyBoid.java
 *
 * Created on 25 May 2007, 17:31
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package boids;

import java.awt.geom.Ellipse2D;
import java.awt.geom.Rectangle2D;
import java.util.ArrayList;

/**
 *
 * @author Olner Dan
 */
public class PredatorBoid extends Boid {
    
    private ArrayList<Boid> foodList = new ArrayList<Boid>();
    
    private Ellipse2D.Double mouth;
    
    
    /** Creates a new instance of PredatorBoid
     * the no-args constructor makes a new random boid at the start of evolution
     * the boidsarray one is for creating a new-born - it passes in a reference
     * to the entire population so's the evolution class can decide who breeds
     */
    
    public PredatorBoid(int id, int[] cageXY, Evolution evolution, int turningSpeed, int[] fovSize) {
        
        super(id, cageXY, evolution, turningSpeed, fovSize);
        
    }
    
      /*This is the constructor for birthing a new Boid...
       */
    public PredatorBoid(boolean newBoid, int id, int[] cageXY, Evolution evolution, int[] fovSize) {
        
        super(newBoid, id, cageXY, evolution, fovSize);
        
    }
    
    
    //Check if any prey boids are near enough to have a go at. If they are,
    //I 'register my interest' with them (in the form of how close they are to me)
    //And leave it up to them to decide who eats them!
    //will only be called if the boid knows one of its directionRules' fields of view
    //has seen a boidy.  And we only need to check rules 0, 1 and 2 coz they're the ones that
    //detect preyboids...
    public void distanceToScoff(){
        
        //check the three preyboid field of view rules
        //Note that the virtual check has already been done for us -
        //Any boids we see, we know we have seen at any point in the torus
        for (int i=0;i<3;i++){
            
            if (dirRule[i].didISeeAny() ) {
                
                for (Boid prey : dirRule[i].boidsInView) {
                    
                    if (prey instanceof PreyBoid && !foodList.contains(prey) ) { foodList.add(prey); }
                    
                }//end dirRule foreach loop
                
            }//END didIseeany if statement
            
        }//END field of view for loop
        
        //System.out.println("PreyBoids nearby to eat: " + foodList.size() );
        
        //Make a new mouth (a circle for which we'll check if any preyboids are in... currently 4 in circumference
        mouth = new Ellipse2D.Double(xyLocation[0].x-5, xyLocation[0].y-5, 10, 10);
        
        //iterate through the preyboids we've seen
        //Note: we have to do a virtual point check, just in case the potential food
        //is just off the edge of the screen. But we know which boids virtual points to 
        //check, so its not too onerous. Its a short-circuit or too, so in all likelihood we'll get an answer for
        //that first. Follow by 2,4,6 and 8 coz they're the largest virtual areas. End with corners.
        for (Boid prey : foodList) {
            
            boolean chomp = false;
            
            if (mouth.contains(prey.xyLocation[0]) || mouth.contains(prey.xyLocation[2]) || mouth.contains(prey.xyLocation[4])  
            || mouth.contains(prey.xyLocation[6]) || mouth.contains(prey.xyLocation[8]) || mouth.contains(prey.xyLocation[1]) 
            || mouth.contains(prey.xyLocation[2]) || mouth.contains(prey.xyLocation[3]) || mouth.contains(prey.xyLocation[4]) )
            
            {
                //then I have a potential dinner! I choose which one to try and eat fairly
                //arbitrarily: whichever is first in the array. I could find the nearest, but that
                //means lots of trig loops, or I could register with each, then see... but that
                //would get *really* complicated. This is relatively straightforward. Visualise it
                //As the predatorBoid lunging and getting lucky...
                
                //Register my interest with the boid in question, in the politest possible terms that
                //(if they don't mind and they're not busy) I'd very much like to scoff them.
                prey.iAmDinnerForWho(this);
                
                //Test: work out distance between me and this prey - is it where I think it is?
                
                double testx = xyLocation[0].x - prey.xyLocation[0].x;
                double testy = xyLocation[0].y - prey.xyLocation[0].y;
                
                double testDist = (Math.sqrt((testx*testx)+(testy*testy)) );
                
               System.out.println("Distance to my selected prey is: " + testDist);
                
            }
            
        }
        
        //and don't forget to finish off by emptying your foodList for this go...
        foodList.clear();
        
    }
    
    //if I've managed to chomp a boid, I need to give myself some energy
    //and have a few turns off eating...
    public void eat() {
        
        //System.out.println("I scoffed and my metabolism was:"+metabolism);
        metabolism+=100;
        //System.out.println("I scoffed and my metabolism was:"+metabolism);
        
        digestion=10;
        
    }
    
    
    //allows others to check whether this is a predator or prey
    public boolean isPredator() {
        
        return true;
        
    }
    
    //Things to do before the next round
    public void nextMoment() {
        
        //Reset all the Field of View arrays for the next go
        //I forgot to do this before, and the poor little buggers have a right fit...
        //empty all the dirRule seens
        for (DirectionRule d : dirRule) {
            
            d.emptyBoidsInView();
            
        }
        
        age++;
        
        //System.out.println("Current age:" + age);
        metabolism--;
        //System.out.println("Current metabolism:" + metabolism);
        
        
        
        if(digestion>0) {digestion--;}
        
        //System.out.println("Predator digestion = " + digestion);
        
        //if I've ran out of energy, I've copped it... time to make a new predator boidy
        if (metabolism==0) {
            
            //System.out.println("I died! Metabolism = 0 and my age is " + age);
            
            PredatorBoid babyBoid = new PredatorBoid(true, id, cageXY, evolution, fovSize);
            
            evolution.EvolveNow(babyBoid, this);
            
        }
        
        
    }
    
    //overwritten but not used here...
    public void iAmDinnerForWho(Boid b){}
    
    
    
}
