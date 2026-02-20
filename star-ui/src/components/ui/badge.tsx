import { type VariantProps, cva } from 'class-variance-authority'
import { cn } from '@/lib/utils'

const badgeVariants = cva(
  'inline-flex items-center gap-1 rounded-full px-2 py-0.5 text-xs font-semibold transition-colors',
  {
    variants: {
      variant: {
        default: 'bg-slate-700 text-slate-200',
        success: 'bg-emerald-900/60 text-emerald-300 border border-emerald-700/40',
        warning: 'bg-amber-900/60 text-amber-300 border border-amber-700/40',
        danger: 'bg-red-900/60 text-red-300 border border-red-700/40',
        info: 'bg-blue-900/60 text-blue-300 border border-blue-700/40',
        outline: 'border border-slate-600 text-slate-300',
      },
    },
    defaultVariants: {
      variant: 'default',
    },
  }
)

interface BadgeProps
  extends React.HTMLAttributes<HTMLSpanElement>,
    VariantProps<typeof badgeVariants> {}

export function Badge({ className, variant, ...props }: BadgeProps) {
  return <span className={cn(badgeVariants({ variant }), className)} {...props} />
}
