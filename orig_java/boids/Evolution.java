/*
 * Evolution.java
 *
 * Created on 25 May 2007, 18:36
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package boids;

import java.awt.geom.AffineTransform;
import java.awt.geom.GeneralPath;
import java.awt.geom.Point2D;
import java.util.ArrayList;

/**
 *
 * @author Olner Dan
 */
public class Evolution {
    
    /** Creates a new instance of Evolution:
     * a reference to BoidsCage is put in so's we can access its
     * array of boids to choose parents and replace the dead one.
     */
    
    BoidsCage boidCage;
    
    //ArrayList for holding potential gene donors...
    ArrayList<Boid> genePool;
    SortArray  sortBoids = new SortArray();
    
    //The various placeholders for getting and using the parent's gene data
    Boid boid1,boid2;
    AffineTransform tx;
    
    //Used to create the babyBoid's fields of vision
    GeneralPath bob;
    
    
    public Evolution(BoidsCage b) {
        
        boidCage = b;
        
    }
    
    
    /* This is where the new baby boid gets its genetic makeup...
     * the babyBoid that's passed in contains a set of null variables
     * ready to be assigned fresh values, including six directionRules
     * Covering 3 for each type of boid.
     */
    
    public void EvolveNow(Boid babyBoid, Boid deadBoid) {
        
        genePool = new ArrayList<Boid>();
        
        if (babyBoid.isPredator() ) {
            
            //System.out.println("Baby boid is predator.");
            boidCage.bd.setPredatorDeathNumber();
            
        } else {
            
            //System.out.println("Baby boid is prey.");
            boidCage.bd.setPreyDeathNumber();
            
        }
        
        //if(deadBoid.isPredator()) {System.out.println("Predator died.");} else {System.out.println("Preyboid died");}
        
        
        //First, take the ages of all my kind of boids, minus myself of course, and add 'em up...
        for (Boid b : boidCage.boids) {
            
            if (deadBoid.id != b.id) {
                
                //check they're the same breed...
                if (b.getClass() == babyBoid.getClass()) {genePool.add(b); 
                //System.out.println("Class chosen is:" +b.getClass() + " and baby is "+babyBoid.getClass() );
                }
                
            }
            
        }
        
        //Sort the ArrayList into order, arranged by age, oldest first
        //Adapted from bubblesort algorithm at
        //http://www.daniweb.com/code/snippet18.html
        for (int a = 0; a < genePool.size(); a++) {
            
            for (int b = 0; b < genePool.size()-1; b++) {
                
                boid1 = genePool.get(b);
                boid2 = genePool.get(b+1);
                
                if(boid1.age < boid2.age) {
                    
                    //temp = array[b];
                    //array[b] = array[b + 1];
                    //array[b + 1] = temp;
                    genePool.set(b,boid2);
                    
                    genePool.set(b+1,boid1);
                    
                }
            }
            
        }
        
        //check to see if age range is changing...
        for (Boid b : genePool) {
            
            System.out.println("Age of genepool boid:" + b.age);
            
        }
        
        //Now pick two to act as parents to the new chick
        //First thing, choose out of the boids we have available.  A cube-root function allows
        //us to give a big bias towards the older birds without ruling out any from the gene pool
        //Run this twice to get two boids
        double root = Math.cbrt(genePool.size() );
        int answer;
        
        double random = Utils.r.nextDouble();
        random*=root;
        
        //Cube again
        double temprandom = random;
        random *= random; random *= temprandom;
        
        //cast it into a round number
        answer = (int)random;
        
        //Our first lucky parent...
        boid1 = genePool.get(answer);
        
        //And repeat (quickest way, less faffing...)
        random = Utils.r.nextDouble();
        random*=root;
        
        //Cube again
        temprandom = random;
        random *= random; random *= temprandom;
        
        //cast it into a round number
        answer = (int)random;
        
        //And parent number 2!
        boid2 = genePool.get(answer);
        
        
        //Now starting divvying out the genes. Non-field of vision ones are:
        //directionMomentum
        //speedInertia
        //In the six dirRules:
        //Speed [length of path]
        //Rule weight
        //Relative angle
        //Vector reaction speed
        //Vector rule weight
        //Vector angle
        
        double chooseGene;
        
        //first the boid-level genes
        chooseGene = Utils.r.nextDouble();
        
        babyBoid.directionMomentum = (chooseGene <0.5? boid1.directionMomentum: boid2.directionMomentum);
        
        chooseGene = Utils.r.nextDouble();
        babyBoid.speedInertia = (chooseGene <0.5? boid1.speedInertia : boid2.speedInertia);
        
        //then the directionRule genes
        for (int i=0;i<6;i++) {
            
            chooseGene = Utils.r.nextDouble();
            babyBoid.dirRule[i].relativeAngle = (chooseGene <0.5? boid1.dirRule[i].relativeAngle : boid2.dirRule[i].relativeAngle);
            
            chooseGene = Utils.r.nextDouble();
            babyBoid.dirRule[i].ruleweight = (chooseGene <0.5? boid1.dirRule[i].ruleweight : boid2.dirRule[i].ruleweight);
            
            chooseGene = Utils.r.nextDouble();
            babyBoid.dirRule[i].speed = (chooseGene <0.5? boid1.dirRule[i].speed : boid2.dirRule[i].speed);
            
            chooseGene = Utils.r.nextDouble();
            babyBoid.dirRule[i].vectorrelativeAngle = 
                    (chooseGene <0.5? boid1.dirRule[i].vectorrelativeAngle : boid2.dirRule[i].vectorrelativeAngle);
            
            chooseGene = Utils.r.nextDouble();
            babyBoid.dirRule[i].vectorruleweight = 
                    (chooseGene <0.5? boid1.dirRule[i].vectorruleweight : boid2.dirRule[i].vectorruleweight);
            
            chooseGene = Utils.r.nextDouble();
            babyBoid.dirRule[i].vectorspeed = (chooseGene <0.5? boid1.dirRule[i].vectorspeed : boid2.dirRule[i].vectorspeed);
            
            
            //the boid.dirRule[i].pathGenes array holds the eight points of the field of view of each parent
            //as they were originally aligned, that is at 0 radians, facing east.
            //We just need to cycle through these, selecting one at random for our baby boid
            
            for (int j=0;j<8;j++) {
                
                babyBoid.dirRule[i].pathGenes[j] = new Point2D.Double();
                
                chooseGene = Utils.r.nextDouble();
                babyBoid.dirRule[i].pathGenes[j] = (chooseGene<0.5 ? boid1.dirRule[i].pathGenes[j] : boid2.dirRule[i].pathGenes[j]);
                
                
            }
            
            
        }
        
        
        //lets leave mutation for now, see what happens...
        
        //Things left to do:
        //1. Give it the old Boid's ID number
        //2. Create its GeneralPath shapes from its new Field of View genes
        //3. Place the new boid into the array slot of boids where the old one was
        
        //babyBoid.id = deadBoid.id;
        
        //Create the GeneralPath objects to represent the field of view...
        //And move them to the right position and rotation...
        //(They will start out at 0,0 as their centre point because they take their
        //co-ordinates from the parents genes, not their position
        for (int i=0;i<babyBoid.dirRule.length;i++) {
            
            //Create the path of the field of view
            for (int j=0;j<8;j++) {
                
                if (j==0) {babyBoid.dirRule[i].fieldOfView.moveTo((float)babyBoid.dirRule[i].pathGenes[j].x, (float)babyBoid.dirRule[i].pathGenes[j].y);}
                
                else {babyBoid.dirRule[i].fieldOfView.lineTo((float)babyBoid.dirRule[i].pathGenes[j].x, (float)babyBoid.dirRule[i].pathGenes[j].y);}
                
            }
            
            babyBoid.dirRule[i].fieldOfView.closePath();
            
            
            tx = new AffineTransform();
            tx.rotate(babyBoid.currentDirection);
            
            babyBoid.dirRule[i].fieldOfView.transform(tx);
            
            //Translate it to the boid's position...
            tx = new AffineTransform();
            tx.translate(babyBoid.xyLocation[0].x,babyBoid.xyLocation[0].y);
            babyBoid.dirRule[i].fieldOfView.transform(tx);
            
        }
        
        //Now stick the fully formed baby boid into the array where the dead one was...
        
        
        
        boidCage.boids[deadBoid.id] = babyBoid;
        
    }
    
    
    
    
    
    
}






/*This is old code, when I was going to rotate and re-rotate fields of view to get their genes
 *Instead now there's a local array in the rule created at birth that holds those genes...
 *
 *Now for the fields of view. First thing we need to do is get all the numbers from the field of views
            //We have to rotate the two parents' 'fields of view' generalpaths back to 0
            //get the numbers from it, and then rotate them back to where they were
            //before we can use their numbers as genes
 
            //Rotate them both back to zero degrees
            //Note that 'movefieldofview' happens at the same time the angle changes, so
            //currentDirection 'should' work...
            tx = new AffineTransform();
            tx.rotate( (boid1.currentDirection)*-1,boid1.xyLocation[0].x, boid1.xyLocation[0].y) ;
            boid1.dirRule[i].fieldOfView.transform(tx);
 
            //repeat for other parent
            tx = new AffineTransform();
            tx.rotate( (boid2.currentDirection)*-1,boid2.xyLocation[0].x, boid2.xyLocation[0].y) ;
            boid1.dirRule[i].fieldOfView.transform(tx);
 
 
            PathIterator geoff = boid1.dirRule[i].fieldOfView.getPathIterator(null);
 
            for (int j=0;j<8;j++) {
 
                geoff.currentSegment(coords);
 
                System.out.println("Segment coords=: "+coords[0] +","+coords[1]);
 
                geoff.next();
 
 
            }*/
