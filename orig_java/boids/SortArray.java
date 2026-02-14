/*
 * SortArray.java
 *
 * Created on 30 May 2007, 00:07
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package boids;

import java.util.ArrayList;

/**
 *
 * @author Olner Dan
 * For sorting an array into number order, largest number in the last index
 * adapted from http://www.daniweb.com/code/snippet18.html
 */

public class SortArray {
    
    
    ArrayList<Boid> localArray = new ArrayList<Boid>();
    Boid boid, boid1;
    
    /** Creates a new instance of SortArray
     *Taking in an array. Note, array is an object
     *so this actually changes the original;
     *no need to return it
     */
    
    
    public SortArray() {
        
        
        
    }
    
    public void SortIt(int[] array) {
        
        int temp;
        
        for (int a = 0; a < array.length; a++) {
            
            for (int b = 0; b < array.length-1; b++)
                
            {
                
                if(array[b] < array[b + 1]) {
                    
                    temp = array[b];
                    
                    array[b] = array[b + 1];
                    
                    array[b + 1] = temp;
                    
                }
                
            }
            
            
        }
        
        for (int a : array) {
            
            System.out.println("Array sorted thus:" + a);
            
        }
        
        
        
        
    }
    
    
    
    /*Version of sort array specifically for taking in an ArrayList
     *And using the contained boid's age field to sort them in order
     */
    public void SortTheBoids(ArrayList<Boid> array) {
    
        localArray = array;
    
        int temp;
    
        for (int a = 0; a < localArray.size(); a++) {
    
            for (int b = 0; b < localArray.size()-1; b++) {
    
                boid = localArray.get(b);
                boid1 = localArray.get(b+1);
    
                if(boid.age < boid1.age) {
    
                    //temp = array[b];
                    //array[b] = array[b + 1];
                    //array[b + 1] = temp;
                    localArray.set(b,boid1);
    
                    localArray.set(b+1,boid);
    
                }
            }
    
            for (Boid bd : localArray) {
    
                System.out.println("Array sorted thus:" + bd.age);
    
            }
    
        }
    
    }
    
    
    
    
    
    
    
}
