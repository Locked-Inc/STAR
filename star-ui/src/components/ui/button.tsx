import { Slot } from '@radix-ui/react-slot'
import { type VariantProps, cva } from 'class-variance-authority'
import { cn } from '@/lib/utils'

const buttonVariants = cva(
  'inline-flex items-center justify-center gap-2 whitespace-nowrap rounded-md text-sm font-medium transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-indigo-500 disabled:pointer-events-none disabled:opacity-50',
  {
    variants: {
      variant: {
        default: 'bg-indigo-600 text-white hover:bg-indigo-500 active:bg-indigo-700',
        destructive: 'bg-red-700 text-white hover:bg-red-600 active:bg-red-800',
        outline: 'border border-slate-600 bg-transparent text-slate-300 hover:bg-slate-800 hover:text-white',
        ghost: 'bg-transparent text-slate-300 hover:bg-slate-800 hover:text-white',
        success: 'bg-emerald-700 text-white hover:bg-emerald-600',
        warning: 'bg-amber-700 text-white hover:bg-amber-600',
      },
      size: {
        default: 'h-9 px-4 py-2',
        sm: 'h-7 rounded px-3 text-xs',
        lg: 'h-11 rounded-md px-8',
        icon: 'h-9 w-9',
      },
    },
    defaultVariants: {
      variant: 'default',
      size: 'default',
    },
  }
)

interface ButtonProps
  extends React.ButtonHTMLAttributes<HTMLButtonElement>,
    VariantProps<typeof buttonVariants> {
  asChild?: boolean
}

export function Button({ className, variant, size, asChild = false, ...props }: ButtonProps) {
  const Comp = asChild ? Slot : 'button'
  return <Comp className={cn(buttonVariants({ variant, size, className }))} {...props} />
}
