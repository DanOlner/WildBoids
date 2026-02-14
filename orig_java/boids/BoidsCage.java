/*
 * BoidsCage.java
 *
 * Created on 23 May 2007, 22:54
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package boids;

import java.awt.Color;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.geom.AffineTransform;
import java.awt.geom.GeneralPath;
import javax.swing.*;

/**
 *
 * @author Olner Dan
 */
public class BoidsCage implements Runnable {

    //Instance variables
    //First is where the little boidies live...
    public Boid[] boids;
    int numberOfBoids;
    DrawPanel panel;
    Thread panelThread;
    boolean boidsReady = false;
    Buffer buffer;
    BoidsStartUp bd = null;
    //Things for painting with
    GeneralPath boid;
    GeneralPath fieldOfView;
    AffineTransform tx;
    //Tests for virtual point detection
    boolean[] rectTest = new boolean[8];
    //test for average direction
    double[] avDir = new double[4];

    BoidsCage(BoidsStartUp bd) {

        this.bd = bd;

        

        //Assign references to variables...
        numberOfBoids = bd.boidsNumber;

        //b*2 because equal number of predator and prey
        boids = new Boid[numberOfBoids * 2];

    //ArrayList<Boid> boids = this.boids;

    }

    /** Creates a new instance of BoidsCage */
    public void run() {

        for (double i : avDir) {
            i = 0;
        }

        //We can pass a reference to this object into a new
        //Evolution class, so that it can access all current Boids to choose parents...
        Evolution evolution = new Evolution(this);

        //Create the boids!
        //We do this before creating the frame, so that the first instance of DrawPanel
        //is ready to draw the first lot of boids immediately
        for (int i = 0; i < numberOfBoids * 2; i += 2) {

            System.out.println("ID number will equal:" + i);

            //the constructor will give each boid random genes...
            //passing in the cage dimensions to randomise its position
            //We also pass in its index number as its ID.
            boids[i] = new PreyBoid(i, bd.CAGE_XY, evolution, bd.turningSpeed, bd.FIELDOFVIEWMAXRADIUS);
            boids[i + 1] = new PredatorBoid(i + 1, bd.CAGE_XY, evolution, bd.turningSpeed, bd.FIELDOFVIEWMAXRADIUS);
            System.out.println("fov: " + bd.FIELDOFVIEWMAXRADIUS[0]);

        }

        System.out.println("Boids all born...");

        //create a buffer: this is used to deal with the problem of
        //checking when boids' fields of vision go off the edge of
        //the torus
        //returns an array with four booleans in for the four buffer zones

        //Buffer needs to be the same as the largest field of view
        //The max is 300, but we can check
        int maxFoV=0;

        for (int FoV : bd.FIELDOFVIEWMAXRADIUS) {

            if (FoV > maxFoV) {
                maxFoV = FoV;
            }

        }

        System.out.println("maxFoV: " + maxFoV);
        
        buffer = new Buffer(bd.CAGE_XY,maxFoV);

        //set the boidcage's frame up...

        JFrame frame = new JFrame();

        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        //Start drawing panel on a different thread, passing in a ref to this object...
        panel = new DrawPanel();
        //panelThread = new Thread(panel);
        //panelThread.start();

        frame.getContentPane().add(panel);

        frame.setSize(bd.CAGE_XY[0], bd.CAGE_XY[1]);
        frame.setLocation(200, 200);
        frame.setVisible(true);

        //start the show...
        go();

    }

    public void go() {

        //Start checking each boid's fields of vision. Note again:
        //direction rule 0,1 and 2 are for PreyBoids
        //direction rule 3,4 and 5 are for PredatorBoids

        while (true) {

            //cycle through each boid
            for (int i = 0; i < boids.length; i++) {


                //First get a boolean for if they're in any of the buffer zones
                //Best to do it here; a little less calculation than doing a
                //'contains' check repeatedly for each field of vision rule. I hope
                boolean inRectA = false;
                boolean inRectB = false;
                boolean inRectC = false;
                boolean inRectD = false;

                if (buffer.RectA.contains(boids[i].xyLocation[0])) {
                    inRectA = true;
                }
                if (buffer.RectB.contains(boids[i].xyLocation[0])) {
                    inRectB = true;
                }
                if (buffer.RectC.contains(boids[i].xyLocation[0])) {
                    inRectC = true;
                }
                if (buffer.RectD.contains(boids[i].xyLocation[0])) {
                    inRectD = true;
                }


                //cycle through each of the boid's 6 fields of vision
                for (int j = 0; j < 6; j++) {

                    //cycle through every other boid to see if its inside this field of vision
                    for (int k = 0; k < boids.length; k++) {

                        //Check we're not checking ourselves
                        if (k != i) {

                            //is this boid in my field of vision?
                            //check to see also if we need to check its virtual points...
                            //If we find any, make 'foundBoid' true, and add its real-point
                            //version to my list of perceived boids
                            //xyLocation is an array of points where 0 is the real point
                            //and 1 to 8 are virtual points to test position on edges
                            //of the torus. See dirRule class for more details.

                            boolean foundBoid = false;

                            //Do main rule test
                            if (boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[0])) {
                                foundBoid = true;
                            }



                            //Do four rectangle checks first;
                            if (inRectA && boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[2])) {
                                foundBoid = true;
                                rectTest[1] = true;
                            }

                            if (inRectB && boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[8])) {
                                foundBoid = true;
                                rectTest[7] = true;
                            }


                            if (inRectC && boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[6])) {
                                foundBoid = true;
                                rectTest[5] = true;
                            }

                            if (inRectD && boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[4])) {
                                foundBoid = true;
                                rectTest[3] = true;
                            }

                            //then four corner checks, which is the overlap points of our rectangles...
                            //top-left is A n B
                            if (inRectA && inRectB && boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[1])) {
                                foundBoid = true;
                                rectTest[0] = true;
                            }
                            //top right is A n D
                            if (inRectA && inRectD && boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[3])) {
                                foundBoid = true;
                                rectTest[2] = true;
                            }
                            //bottom right is C n D
                            if (inRectC && inRectD && boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[5])) {
                                foundBoid = true;
                                rectTest[4] = true;
                            }
                            //bottom left is C n B
                            if (inRectC && inRectB && boids[i].dirRule[j].fieldOfView.contains(boids[k].xyLocation[7])) {
                                foundBoid = true;
                                rectTest[6] = true;
                            }

                            /*For testing...
                            System.out.println("I detected a virtual point in RectCB and my XY=:"
                            +boids[i].xyLocation[0].x+","+boids[i].xyLocation[0].y+"");
                            System.out.println("The virtual point is at: "+boids[k].xyLocation[7].x+","+boids[k].xyLocation[7].y+"\n");
                            System.out.println("and corresponds to absolute point:"+boids[k].xyLocation[0].x+","+boids[k].xyLocation[0].y+"\n");*/


                            //
                            if (foundBoid) {

                                //First let the boid know its seen something...
                                boids[i].foundBoid();

                                //Then add it to my rule's list (if the appropriate type of boid)
                                //Rules 0,1,2 are PreyBoid rules, hence this check...
                                if (boids[i] instanceof PreyBoid && j < 3) {

                                    //add the boid! (If a preyboid)
                                    boids[i].dirRule[j].boidsInView(boids[k]);

                                } else {//add the boid! (If a predatorboid)
                                    boids[i].dirRule[j].boidsInView(boids[k]);
                                }

                            } //END field of vision check

                        }// END if statement

                    }// END k loop

                }// END j loop

            }// END i loop




            //Fly, boids, fly!
            for (int i = 0; i < boids.length; i++) {

                if (boids[i].didIFindBoid()) {

                    //First things, PredatorBoids need to check to see if there's a boid near enough to be scoffed
                    //Now they know there's one near, they can use their 'mouth' (a small circle) to check the
                    //near vicinity, by looking the directionRules ArrayList of seen boids...
                    //If digestion > 0 it means its still eating the last one. The point here being to stop it cutting
                    //through swathes of boids in one fell, er, swoop...
                    if (boids[i] instanceof PredatorBoid && boids[i].digestion == 0) {

                        //checks if any boid is within scoffing distance.
                        //If any are, it 'registers its interest' in eating it!
                        boids[i].distanceToScoff();


                    }


                    //Say 'OK' and turn off the didIFindBoid for next time
                    boids[i].doneFoundBoid();

                    //System.out.println("I saw a boidy!");

                    //Don't forget to empty the BoidsInView array when you're done here!

                    //Now I have to work out a new heading for myself, based on all the six rules...
                    //The rules - if they found anything - have a list of boids and their average distance

                    for (int j = 0; j < boids[i].dirRule.length; j++) {

                        if (boids[i].dirRule[j].didISeeAny()) {
                            //then calculate a vector for this one.



                            boids[i].setNewHeading();
                        //boids[i].repeatPreviousHeading();

                        }

                    }



                } else {
                    boids[i].repeatPreviousHeading();
                }

            /*test average angles
            if ( (boids[i].currentDirection>0) && (boids[i].currentDirection<(Math.toRadians(90)))) {avDir[0]++;} else
            if (boids[i].currentDirection>(Math.PI/2) && boids[i].currentDirection<(Math.PI)) {avDir[1]++;} else
            if (boids[i].currentDirection>(Math.PI) && boids[i].currentDirection<((Math.PI/2)*3)) {avDir[2]++;} else
            if (boids[i].currentDirection>((Math.PI)/2)*3 && boids[i].currentDirection<((Math.PI*2))) {avDir[3]++;}
            for (double av : avDir) {
            System.out.println("From 0 to 3: " + av);
            } System.out.println("\n");*/


            }//End of main boids i loop

            //Do all the necessary checks before the next round...

            for (Boid b : boids) {

                b.nextMoment();

            }


            panel.repaint();

            try {
                Thread.sleep(bd.displaySpeed);
            } catch (Exception ex) {
                ex.printStackTrace();
            }

        //testing choose...


        }//END OF MAIN WHILE LOOP





    }// END of GO METHOD ////
    /**
     * inner class for drawing the flock...
     * It has the following jobs, and these only
     * 1. Take in a set of points and directions to plot the position of
     * a. Prey boids
     * b. Predator boids
     * 2. Check if a boid is dead; plot it accordingly
     */
    class DrawPanel extends JPanel {

        public void paint(Graphics g) {


            Graphics2D g2 = (Graphics2D) g;

            super.paint(g2);

            //Plot the position of all the boids... er, different colours for different types?
            //Preyboids are odd numbers, Predators are even...

            //Preyboids first...


            for (int i = 0; i < boids.length; i++) {

                if (boids[i] instanceof PreyBoid) {
                    g2.setColor(Color.GREEN);
                } else {
                    g2.setColor(Color.RED);
                }

                //create our boid shape, facing 0 radians...
                boid = new GeneralPath();

                boid.moveTo(10, 0);
                boid.lineTo(-10, 7);
                boid.lineTo(0, 0);
                boid.lineTo(-10, -7);
                boid.closePath();

                //Now move the shape to the boid's position and rotation
                tx = new AffineTransform();

                tx.translate(boids[i].xyLocation[0].x, boids[i].xyLocation[0].y);
                boid.transform(tx);

                tx = new AffineTransform();

                tx.rotate(boids[i].currentDirection, boids[i].xyLocation[0].x, boids[i].xyLocation[0].y);
                boid.transform(tx);

                g2.draw(boid);



                //Then see if the user wants any of the field of view rules drawn
                if (bd.showFieldOfView != 0) {

                    //showFieldOfView is -1 because in the original value,
                    //0 = 'display' no field of view rules...

                    //Colours: for seeing preyboid = green, for seeing prey = red
                    //to match the colour they're looking for

                    //first check if I saw any with this rule; if so, show...

                    if (!boids[i].dirRule[bd.showFieldOfView - 1].didISeeAny()) {

                        if (bd.showFieldOfView < 4) {
                            g2.setColor(Color.GREEN);
                        } else {
                            g2.setColor(Color.RED);
                        }

                        //else {g2.setColor(Color.BLACK);}

                        //System.out.println("Should be setting white...");

                        g2.draw(boids[i].dirRule[bd.showFieldOfView - 1].fieldOfView);

                    }


                }


                //test of total new heading

                /*for (i=0;i<6;i++) {
                //System.out.println("Total new heading reading: x="+p.x+",y="+p.y);
                double x = ((boids[0].totalNewHeading[i].x)*10)+boids[0].xyLocation[0].x;
                double y = ((boids[0].totalNewHeading[i].y)*10)+boids[0].xyLocation[0].y;
                //double x = (p.x)*10+400;
                //double y = (p.y)*10+400;
                //if(i<4) {g2.setColor(Color.GREEN);} else {g2.setColor(Color.RED);}
                g2.fillOval ((int)x,(int)y,10,10);
                }*/



                //Temp: draw a circle around boid 0:

                g2.setColor(Color.BLUE);
                g2.drawOval((int) boids[0].xyLocation[0].x - 20, (int) boids[0].xyLocation[0].y - 20, 40, 40);


            }





        }
    }
}
